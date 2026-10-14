#ifndef MWGUI_MESSAGE_BOX_H
#define MWGUI_MESSAGE_BOX_H

#include <memory>

#include "windowbase.hpp"

namespace MyGUI
{
    class Widget;
    class Button;
    class EditBox;
}

namespace MWGui
{
    class InteractiveMessageBox;
    class MessageBoxManager;
    class MessageBox;
    class MessageBoxManager
    {
    public:
        MessageBoxManager(float timePerChar);
        ~MessageBoxManager();
        void onFrame(float frameDuration);
        void createMessageBox(std::string_view message, bool stat = false);
        void removeStaticMessageBox();
        /*
            Start of tes3mp change (major)

            Add a hasServerOrigin boolean to the list of arguments so those messageboxes
            can be differentiated from client-only ones
        */
        bool createInteractiveMessageBox(std::string_view message, const std::vector<std::string>& buttons,
            bool immediate = false, int defaultFocus = -1, bool hasServerOrigin = false);
        /*
            End of tes3mp change (major)
        */
        bool isInteractiveMessageBox();

        std::size_t getMessagesCount();

        const InteractiveMessageBox* getInteractiveMessageBox() const { return mInterMessageBoxe.get(); }

        /// Remove all message boxes
        void clear();

        bool removeMessageBox(MessageBox* msgbox);

        /// @param reset Reset the pressed button to -1 after reading it.
        int readPressedButton(bool reset = true);

        void resetInteractiveMessageBox();

        void setLastButtonPressed(int index);

        typedef MyGUI::delegates::MultiDelegate<int> EventHandle_Int;

        // Note: this delegate unassigns itself after it was fired, i.e. works once.
        EventHandle_Int eventButtonPressed;

        void onButtonPressed(int button)
        {
            eventButtonPressed(button);
            eventButtonPressed.clear();
        }

        void setVisible(bool value);

        const std::vector<std::unique_ptr<MessageBox>>& getActiveMessageBoxes() const;

    private:
        std::vector<std::unique_ptr<MessageBox>> mMessageBoxes;
        std::unique_ptr<InteractiveMessageBox> mInterMessageBoxe;
        MessageBox* mStaticMessageBox;
        float mMessageBoxSpeed;
        int mLastButtonPressed;
        bool mVisible = true;
    };

    class MessageBox : public Layout
    {
    public:
        MessageBox(MessageBoxManager& parMessageBoxManager, std::string_view message);
        const std::string& getMessage() { return mMessage; }
        int getHeight();
        void update(int height);
        void setVisible(bool value);

        float mCurrentTime;
        float mMaxTime;

    protected:
        MessageBoxManager& mMessageBoxManager;
        std::string mMessage;
        MyGUI::EditBox* mMessageWidget;
        int mBottomPadding;
        int mNextBoxPadding;
    };

    class InteractiveMessageBox : public WindowModal
    {
    public:
        friend class MessageBoxManager;
        InteractiveMessageBox(MessageBoxManager& parMessageBoxManager, const std::string& message,
            const std::vector<std::string>& buttons, bool immediate, size_t defaultFocus);
        void mousePressed(MyGUI::Widget* widget);
        int readPressedButton();

        MyGUI::Widget* getDefaultKeyFocus() override;

        bool exit() override { return false; }

        bool mMarkedToDelete;

        bool onControllerButtonEvent(const SDL_ControllerButtonEvent& arg) override;

    private:
        void buttonActivated(MyGUI::Widget* widget);
            /*
                Start of tes3mp addition

                Track whether the message box has a server origin
            */
            bool mHasServerOrigin = false;
            /*
                End of tes3mp addition
            */

        MessageBoxManager& mMessageBoxManager;
        MyGUI::EditBox* mMessageWidget;
        MyGUI::Widget* mButtonsWidget;
        std::vector<MyGUI::Button*> mButtons;

        int mButtonPressed;
        size_t mDefaultFocus;
        bool mImmediate;
        size_t mControllerFocus = 0;
    };

}

#endif
