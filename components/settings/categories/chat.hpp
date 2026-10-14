#ifndef OPENMW_COMPONENTS_SETTINGS_CATEGORIES_CHAT_H
#define OPENMW_COMPONENTS_SETTINGS_CATEGORIES_CHAT_H

#include <components/settings/sanitizerimpl.hpp>
#include <components/settings/settingvalue.hpp>

#include <string>

namespace Settings
{
    struct ChatCategory : WithIndex
    {
        using WithIndex::WithIndex;

        SettingValue<float> mDelay{ mIndex, "Chat", "delay" };
        SettingValue<int> mX{ mIndex, "Chat", "x" };
        SettingValue<int> mY{ mIndex, "Chat", "y" };
        SettingValue<int> mW{ mIndex, "Chat", "w" };
        SettingValue<int> mH{ mIndex, "Chat", "h" };
        SettingValue<std::string> mKeySay{ mIndex, "Chat", "keySay" };
        SettingValue<std::string> mKeyChatMode{ mIndex, "Chat", "keyChatMode" };
    };
}

#endif
