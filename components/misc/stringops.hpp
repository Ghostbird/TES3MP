#ifndef MISC_STRINGOPS_H
#define MISC_STRINGOPS_H

// Compatibility header for TES3MP: the string utilities have been split into
// modular headers in OpenMW 0.52. This header includes all of them so that
// existing #include <components/misc/stringops.hpp> directives keep working.

#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/conversion.hpp>
#include <components/misc/strings/format.hpp>
#include <components/misc/strings/lower.hpp>
#include <components/misc/utf8stream.hpp>

namespace Misc::StringUtils
{
    inline Utf8Stream::UnicodeChar toLowerUtf8(Utf8Stream::UnicodeChar ch) { return Utf8Stream::toLowerUtf8(ch); }
    inline std::string lowerCaseUtf8(std::string_view str) { return Utf8Stream::lowerCaseUtf8(str); }
}

#endif
