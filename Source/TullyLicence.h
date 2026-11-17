// TullyLicence.h - purchase activation for Tully EDM Vibe plugins.
//
// The first time a plugin opens it asks for the email address the buyer paid with. The licence service
// (tully-licence.nrbessmer.workers.dev) checks Stripe for a paid purchase of this product by that email,
// allows 3 computers per purchase, and returns a licence signed with Ed25519. The licence is tied to this
// Mac (its hardware UUID, hashed) and kept in a private file:
//   ~/Library/Application Support/Tully EDM Vibe/Licences/<product>.lic
// After that the plugin works offline. A licence copied to another Mac does not verify there.
//
// Use: in the processor constructor   tully::licence ("Bass Professor");
//      at the top of processBlock     if (! tully::licence().ok()) { ...silence or pass through...; return; }
//      at the end of the editor ctor  tully::addActivation (*this);
// (c)(p) 2026 Tully EDM Vibe - info@tullyedmvibe.com
#pragma once
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "TweetNaclVerify.h"

namespace tully
{
static constexpr const char* kLicenceServer = "https://tully-licence.nrbessmer.workers.dev/activate";
static constexpr const char* kLicencePublicKey = "21d360f184f6005de4da7753767c06cfe56f3991e6013b458689462bcb3fd1da";
static constexpr const char* kSupportEmail = "info@tullyedmvibe.com";

class Licence
{
public:
    explicit Licence (const juce::String& productName) : product (productName)
    {
        // The plugin's own test programs link the processor directly (no AU/VST3/standalone wrapper):
        // they run without a licence. Every shipped build has a wrapper and is checked.
       #ifndef TULLY_FORCE_UNLICENSED
        testHarness = juce::PluginHostType::jucePlugInClientCurrentWrapperType == juce::AudioProcessor::wrapperType_Undefined;
       #endif
        check();
    }

    bool ok() const noexcept { return testHarness || valid.load (std::memory_order_relaxed); }
    const juce::String& productName() const noexcept { return product; }
    juce::String licensedEmail() const { const juce::ScopedLock sl (lock); return email; }

    // This Mac, as the licence service sees it: a hash of the hardware UUID (never the UUID itself).
    static juce::String machineId()
    {
       #ifdef TULLY_TEST_MACHINE
        return TULLY_TEST_MACHINE;   // test builds only
       #endif
        auto id = juce::SystemStats::getUniqueDeviceID();
        if (id.isEmpty()) id = juce::SystemStats::getComputerName() + juce::SystemStats::getLogonName();
        const auto in = ("TEV-machine|" + id).toStdString();
        std::vector<unsigned char> h (64);
        tnacl::crypto_hash (h.data(), (const unsigned char*) in.data(), (unsigned long long) in.size());
        return juce::String::toHexString (h.data(), 16, 0);   // 32 hex characters
    }

    juce::File file() const
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Application Support/Tully EDM Vibe/Licences").getChildFile (product + ".lic");
    }

    // Reads and verifies the licence file. Safe to call from the message thread at any time.
    bool check()
    {
        juce::String e;
        const bool good = verifyJson (juce::JSON::parse (file().loadFileAsString()), e);
        { const juce::ScopedLock sl (lock); email = good ? e : juce::String(); }
        valid.store (good);
        return good;
    }

    // Asks the licence service. done (ok, message) is called on the message thread.
    void activate (const juce::String& buyerEmail, std::function<void (bool, juce::String)> done)
    {
        auto* self = this;
        const auto prod = product, mid = machineId(), mail = buyerEmail.trim();
        juce::Thread::launch ([self, prod, mid, mail, done]
        {
            juce::DynamicObject::Ptr req (new juce::DynamicObject());
            req->setProperty ("email", mail); req->setProperty ("product", prod); req->setProperty ("machine", mid);
            int status = 0;
            const auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                                  .withExtraHeaders ("Content-Type: application/json")
                                  .withConnectionTimeoutMs (20000).withStatusCode (&status);
            juce::String body;
            if (auto in = juce::URL (kLicenceServer).withPOSTData (juce::JSON::toString (juce::var (req.get()), true)).createInputStream (opts))
                body = in->readEntireStreamAsString();
            const auto reply = juce::JSON::parse (body);
            bool good = false; juce::String msg;
            if (body.isEmpty())
                msg = "Could not reach the licence server. Check the internet connection and try again.";
            else if ((bool) reply.getProperty ("ok", false))
            {
                const auto lic = reply.getProperty ("licence", {});
                juce::String e;
                if (self->verifyJson (lic, e))
                {
                    const auto f = self->file();
                    f.getParentDirectory().createDirectory();
                    good = f.replaceWithText (juce::JSON::toString (lic, true));
                    msg = good ? "Activated for " + e + ". Thank you!" : "Could not save the licence file in " + f.getParentDirectory().getFullPathName();
                }
                else msg = "The licence did not check out. Please write to " + juce::String (kSupportEmail) + ".";
            }
            else msg = reply.getProperty ("error", "Activation failed.").toString();
            juce::MessageManager::callAsync ([self, good, msg, done] { if (good) self->check(); if (done) done (good && self->ok(), msg); });
        });
    }

private:
    bool verifyJson (const juce::var& v, juce::String& emailOut) const
    {
        if (! v.isObject()) return false;
        const auto p = v.getProperty ("product", {}).toString(), e = v.getProperty ("email", {}).toString(),
                   m = v.getProperty ("machine", {}).toString(), d = v.getProperty ("issued", {}).toString(),
                   s = v.getProperty ("sig", {}).toString();
        if (! p.equalsIgnoreCase (product) || m != machineId() || s.isEmpty()) return false;
        juce::MemoryOutputStream sig;
        if (! juce::Base64::convertFromBase64 (sig, s) || sig.getDataSize() != 64) return false;
        juce::MemoryBlock pk; pk.loadFromHexString (kLicencePublicKey);
        const auto msg = ("TEV1|" + p + "|" + e + "|" + m + "|" + d).toStdString();
        if (pk.getSize() != 32 || ! tnacl::verify ((const unsigned char*) sig.getData(), (const unsigned char*) msg.data(),
                                                   (unsigned long long) msg.size(), (const unsigned char*) pk.getData()))
            return false;
        emailOut = e;
        return true;
    }

    const juce::String product;
    bool testHarness = false;
    std::atomic<bool> valid { false };
    juce::CriticalSection lock;
    juce::String email;
};

// One licence per plugin binary: the first call names the product (do it in the processor constructor).
inline Licence& licence (const char* productName = nullptr)
{
    static Licence l (productName != nullptr ? productName : "");
    return l;
}

//==============================================================================
// Covers the editor until the plugin is activated.
class ActivationOverlay : public juce::Component, private juce::ComponentListener
{
public:
    explicit ActivationOverlay (juce::Component& editor) : parent (editor)
    {
        const auto prod = licence().productName();
        title.setText ("Activate " + prod, juce::dontSendNotification);
        title.setFont (juce::FontOptions (24.0f, juce::Font::bold));
        title.setJustificationType (juce::Justification::centred);
        info.setText ("Type the email address you bought " + prod + " with. It is checked once, then "
                      + prod + " works offline on this Mac (up to 3 computers per purchase).", juce::dontSendNotification);
        info.setJustificationType (juce::Justification::centred);
        info.setFont (juce::FontOptions (15.0f));
        emailBox.setTextToShowWhenEmpty ("you@example.com", juce::Colours::grey);
        emailBox.setFont (juce::FontOptions (17.0f));
        emailBox.onReturnKey = [this] { go(); };
        button.onClick = [this] { go(); };
        status.setJustificationType (juce::Justification::centred);
        status.setFont (juce::FontOptions (14.0f));
        buy.setButtonText ("Buy " + prod + " at tullyedmvibe.com");
        buy.setURL (juce::URL ("https://www.tullyedmvibe.com/"));
        help.setText ("Help: " + juce::String (kSupportEmail) + "   -   Tully EDM Vibe", juce::dontSendNotification);
        help.setJustificationType (juce::Justification::centred);
        help.setFont (juce::FontOptions (12.0f));
        for (auto* l : { &title, &info, &status, &help }) { l->setColour (juce::Label::textColourId, juce::Colours::white); addAndMakeVisible (l); }
        help.setColour (juce::Label::textColourId, juce::Colour (0xff22d3ee));
        button.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffff2d95));
        addAndMakeVisible (emailBox); addAndMakeVisible (button); addAndMakeVisible (buy);
        setAlwaysOnTop (true);
        setWantsKeyboardFocus (true);
        parent.addComponentListener (this);
        parent.addAndMakeVisible (this);
        setBounds (parent.getLocalBounds());
        toFront (false);
    }
    ~ActivationOverlay() override { parent.removeComponentListener (this); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xe60b0b14));
        g.setColour (juce::Colour (0xff141427)); g.fillRoundedRectangle (card.toFloat(), 14.0f);
        g.setColour (juce::Colour (0xffff2d95).withAlpha (0.8f)); g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 14.0f, 1.5f);
    }
    void resized() override
    {
        const int w = juce::jmin (getWidth() - 24, 520), h = juce::jmin (getHeight() - 24, 300);
        card = getLocalBounds().withSizeKeepingCentre (w, h);
        auto r = card.reduced (20, 14);
        title.setBounds (r.removeFromTop (34));
        info.setBounds (r.removeFromTop (60));
        auto row = r.removeFromTop (36);
        button.setBounds (row.removeFromRight (110)); row.removeFromRight (8);
        emailBox.setBounds (row);
        r.removeFromTop (6);
        status.setBounds (r.removeFromTop (44));
        help.setBounds (r.removeFromBottom (18));
        buy.setBounds (r.removeFromBottom (26));
    }

private:
    void go()
    {
        const auto e = emailBox.getText().trim();
        if (! e.containsChar ('@')) { status.setText ("Please type the email address you bought with.", juce::dontSendNotification); return; }
        button.setEnabled (false); emailBox.setEnabled (false);
        status.setColour (juce::Label::textColourId, juce::Colours::white);
        status.setText ("Checking your purchase...", juce::dontSendNotification);
        juce::Component::SafePointer<ActivationOverlay> safe (this);
        licence().activate (e, [safe] (bool good, juce::String msg)
        {
            if (safe == nullptr) return;
            safe->status.setColour (juce::Label::textColourId, good ? juce::Colour (0xff4ade80) : juce::Colour (0xffff7aa2));
            safe->status.setText (msg, juce::dontSendNotification);
            safe->button.setEnabled (true); safe->emailBox.setEnabled (true);
            if (good) juce::Timer::callAfterDelay (1200, [safe] { if (safe != nullptr) delete safe.getComponent(); });
        });
    }
    void componentMovedOrResized (juce::Component&, bool, bool) override { setBounds (parent.getLocalBounds()); }
    void componentBeingDeleted (juce::Component&) override { delete this; }

    juce::Component& parent;
    juce::Rectangle<int> card;
    juce::Label title, info, status, help;
    juce::TextEditor emailBox;
    juce::TextButton button { "Activate" };
    juce::HyperlinkButton buy;
};

// Shows the activation screen over the editor when this Mac has no licence yet.
inline void addActivation (juce::Component& editor)
{
    if (! licence().ok() && ! licence().check())
        new ActivationOverlay (editor);   // owned by the editor: deletes itself with it, or once activated
}
}
