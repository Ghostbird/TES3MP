#ifndef OPENMW_COMPONENTS_SETTINGS_CATEGORIES_GENERAL_H
#define OPENMW_COMPONENTS_SETTINGS_CATEGORIES_GENERAL_H

#include <components/settings/sanitizerimpl.hpp>
#include <components/settings/settingvalue.hpp>

#include <osg/Math>
#include <osg/Vec2f>
#include <osg/Vec3f>

#include <cstdint>
#include <string>
#include <string_view>

namespace Settings
{
    struct GeneralCategory : WithIndex
    {
        using WithIndex::WithIndex;

        SettingValue<int> mAnisotropy{ mIndex, "General", "anisotropy", makeClampSanitizerInt(0, 16) };
        SettingValue<std::string> mScreenshotFormat{ mIndex, "General", "screenshot format",
            makeEnumSanitizerString({ "jpg", "png", "tga" }) };
        SettingValue<std::string> mTextureMagFilter{ mIndex, "General", "texture mag filter",
            makeEnumSanitizerString({ "nearest", "linear" }) };
        SettingValue<std::string> mTextureMinFilter{ mIndex, "General", "texture min filter",
            makeEnumSanitizerString({ "nearest", "linear" }) };
        SettingValue<std::string> mTextureMipmap{ mIndex, "General", "texture mipmap",
            makeEnumSanitizerString({ "none", "nearest", "linear" }) };
        SettingValue<bool> mNotifyOnSavedScreenshot{ mIndex, "General", "notify on saved screenshot" };
        SettingValue<std::vector<std::string>> mPreferredLocales{ mIndex, "General", "preferred locales" };
        SettingValue<bool> mGmstOverridesL10n{ mIndex, "General", "gmst overrides l10n" };
        SettingValue<std::size_t> mLogBufferSize{ mIndex, "General", "log buffer size" };
        SettingValue<std::size_t> mConsoleHistoryBufferSize{ mIndex, "General", "console history buffer size" };

        // TES3MP server settings
        SettingValue<std::string> mLocalAddress{ mIndex, "General", "localAddress" };
        SettingValue<int> mPort{ mIndex, "General", "port" };
        SettingValue<int> mMaximumPlayers{ mIndex, "General", "maximumPlayers" };
        SettingValue<std::string> mHostname{ mIndex, "General", "hostname" };
        SettingValue<int> mLogLevel{ mIndex, "General", "logLevel" };
        SettingValue<std::string> mPassword{ mIndex, "General", "password" };
        SettingValue<std::string> mPluginsHome{ mIndex, "Plugins", "home" };
        SettingValue<std::string> mPluginsPlugins{ mIndex, "Plugins", "plugins" };
        SettingValue<bool> mMasterEnabled{ mIndex, "MasterServer", "enabled" };
        SettingValue<std::string> mMasterAddress{ mIndex, "MasterServer", "address" };
        SettingValue<int> mMasterPort{ mIndex, "MasterServer", "port" };
        SettingValue<int> mMasterRate{ mIndex, "MasterServer", "rate" };
    };
}

#endif
