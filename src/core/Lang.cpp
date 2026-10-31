/* Lang.cpp - 多语言文本加载器。
 * 语言跟随系统，每个语言一个文件: MOExperienceResources\<locale>.ini。
 * zh 系列无对应文件用 zh-CN，其它语言无对应文件用 en-US。 */
#include <windows.h>
#include "GdiText.h"
#include "Lang.h"

#ifndef LOCALE_SNAME
#define LOCALE_SNAME 0x0000005c
#endif

#define LANG_DIR_NAME       "MOExperienceResources"
#define LANG_FILE_EXT       ".ini"
#define LANG_LOCALE_CAP     32
#define LANG_LOCALE_WCAP    85
#define LANG_DEFAULT_ZH     "zh-CN"
#define LANG_DEFAULT_EN     "en-US"
#define LANG_VALUE_CAP      512

static int  s_Ready = 0;
static int  s_Mode = LANG_MODE_AUTO;
static char s_SystemLocale[LANG_LOCALE_CAP];
static char s_Path[MAX_PATH_LEN];

static void Lang_CopyA(char* dst, int cap, const char* src)
{
    int i = 0;
    if (!dst || cap <= 0) return;
    if (src)
        while (i < cap - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static void Lang_TrimA(char* b)
{
    int i = 0, j = 0;
    unsigned char* u = (unsigned char*)b;
    if (u[0] == 0xEF && u[1] == 0xBB && u[2] == 0xBF) i = 3;
    while (b[i] == ' ' || b[i] == '\t') i++;
    while (b[i] && b[i] != ';') b[j++] = b[i++];
    while (j > 0 && (b[j - 1] == ' ' || b[j - 1] == '\t')) j--;
    b[j] = 0;
}

static int Lang_ValidLocale(const char* s)
{
    int n = 0;
    if (!s || !*s) return 0;
    for (; *s; s++, n++)
    {
        if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') ||
              (*s >= '0' && *s <= '9') || *s == '-' || *s == '_'))
            return 0;
        if (n >= LANG_LOCALE_CAP - 1) return 0;
    }
    return n > 0;
}

static int Lang_LocaleIsChinese(const char* s)
{
    if (!s) return 0;
    return (s[0] == 'z' || s[0] == 'Z') &&
           (s[1] == 'h' || s[1] == 'H') &&
           (s[2] == 0 || s[2] == '-' || s[2] == '_');
}

/* 把系统语言名 (如 zh-CN/en-US) 转成 ANSI 字符串；locale 名本身是 ASCII。 */
static int Lang_LocaleFromLangId(LANGID id, char* out, int cap)
{
    wchar_t w[LANG_LOCALE_WCAP];
    int n, r;
    if (!id || !out || cap <= 0) return 0;
    n = GetLocaleInfoW(MAKELCID(id, SORT_DEFAULT), LOCALE_SNAME,
                       w, LANG_LOCALE_WCAP);
    if (n <= 1) return 0;
    r = WideCharToMultiByte(CP_ACP, 0, w, -1, out, cap, NULL, NULL);
    if (r <= 1) { out[0] = 0; return 0; }
    out[cap - 1] = 0;
    return 1;
}

static void Lang_GetSystemLocale(void)
{
    s_SystemLocale[0] = 0;
    if (!Lang_LocaleFromLangId(GetUserDefaultUILanguage(),
                               s_SystemLocale, sizeof(s_SystemLocale)))
    {
        Lang_LocaleFromLangId(GetSystemDefaultUILanguage(),
                              s_SystemLocale, sizeof(s_SystemLocale));
    }
    Lang_TrimA(s_SystemLocale);
    if (!Lang_ValidLocale(s_SystemLocale))
        Lang_CopyA(s_SystemLocale, sizeof(s_SystemLocale), LANG_DEFAULT_ZH);
}

static void Lang_MakePath(const char* locale, char* path, int cap)
{
    const char* dir = GtGameDir();
    unsigned i;
    if (!path || cap <= 0) return;
    path[0] = 0;
    if (!locale || !locale[0] || !dir[0]) return;

    i = AppendCStr(path, 0, dir, (unsigned)cap);
    i = AppendCh(path, i, '\\', (unsigned)cap);
    i = AppendCStr(path, i, LANG_DIR_NAME "\\", (unsigned)cap);
    i = AppendCStr(path, i, locale, (unsigned)cap);
    AppendCStr(path, i, LANG_FILE_EXT, (unsigned)cap);
}

static int Lang_TryLocale(const char* locale)
{
    char path[MAX_PATH_LEN];
    Lang_MakePath(locale, path, sizeof(path));
    if (!FileExists(path)) return 0;
    Lang_CopyA(s_Path, sizeof(s_Path), path);
    return 1;
}

static void Lang_SelectFile(void)
{
    s_Path[0] = 0;
    if (s_Mode == LANG_MODE_ZH) { Lang_TryLocale(LANG_DEFAULT_ZH); return; }
    if (s_Mode == LANG_MODE_EN) { Lang_TryLocale(LANG_DEFAULT_EN); return; }
    if (Lang_TryLocale(s_SystemLocale)) return;
    if (Lang_LocaleIsChinese(s_SystemLocale))
    {
        if (Lang_TryLocale(LANG_DEFAULT_ZH)) return;
    }
    else
    {
        if (Lang_TryLocale(LANG_DEFAULT_EN)) return;
    }
}

void Lang_EnsureInit(void)
{
    if (s_Ready) return;
    s_Ready = 1;
    if (!g_IniPath[0]) BuildPaths();
    Lang_GetSystemLocale();
    Lang_SelectFile();
    if (g_Log && !s_Path[0]) LogLine("lang: no language file");
}

int Lang_IsChinese(void)
{
    Lang_EnsureInit();
    if (s_Mode == LANG_MODE_ZH) return 1;
    if (s_Mode == LANG_MODE_EN) return 0;
    return Lang_LocaleIsChinese(s_SystemLocale);
}

int Lang_GetMode(void)
{
    Lang_EnsureInit();
    return s_Mode;
}

void Lang_SetMode(int mode)
{
    if (mode < LANG_MODE_AUTO || mode > LANG_MODE_EN) mode = LANG_MODE_AUTO;
    Lang_EnsureInit();
    if (mode == s_Mode) return;
    s_Mode = mode;
    Lang_SelectFile();
}

static int Lang_SameA(const char* a, const char* b)
{
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == 0 && *b == 0;
}

/* Read the UTF-8 bytes directly. GetPrivateProfileStringA converts through
 * the system ANSI code page before returning, which corrupts UTF-8 on Chinese
 * Windows and makes the subsequent UTF-8 decoder see invalid sequences. */
static void Lang_Read(const char* path, const char* section, const char* key,
                      char* buf, int cap)
{
    char data[16384];
    HANDLE h;
    DWORD size, got;
    char current[128];
    unsigned pos, line, end, i, start;
    int inSection;

    if (!buf || cap <= 0) return;
    buf[0] = 0;
    if (!path || !path[0] || !section || !key) return;

    h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    size = GetFileSize(h, NULL);
    if (size == INVALID_FILE_SIZE || size >= sizeof(data) ||
        !ReadFile(h, data, size, &got, NULL) || got != size)
    {
        CloseHandle(h);
        return;
    }
    CloseHandle(h);
    data[size] = 0;

    inSection = 0;
    pos = 0;
    while (pos < size)
    {
        line = pos;
        while (pos < size && data[pos] != '\n') pos++;
        end = pos;
        if (end > line && data[end - 1] == '\r') end--;

        start = line;
        while (start < end && (data[start] == ' ' || data[start] == '\t')) start++;
        if (start < end && data[start] == '[')
        {
            i = start + 1;
            while (i < end && data[i] != ']') i++;
            if (i < end)
            {
                unsigned n = i - start - 1;
                if (n >= sizeof(current)) n = sizeof(current) - 1;
                for (unsigned j = 0; j < n; j++) current[j] = data[start + 1 + j];
                current[n] = 0;
                Lang_TrimA(current);
                inSection = Lang_SameA(current, section);
            }
            else inSection = 0;
        }
        else if (inSection && start < end && data[start] != ';' &&
                 data[start] != '#')
        {
            i = start;
            while (i < end && data[i] != '=') i++;
            if (i < end)
            {
                unsigned n = i - start;
                while (n > 0 && (data[start + n - 1] == ' ' ||
                                 data[start + n - 1] == '\t')) n--;
                if (n < sizeof(current))
                {
                    for (unsigned j = 0; j < n; j++) current[j] = data[start + j];
                    current[n] = 0;
                    if (Lang_SameA(current, key))
                    {
                        unsigned value = i + 1;
                        while (value < end && (data[value] == ' ' ||
                                               data[value] == '\t')) value++;
                        unsigned out = 0;
                        while (value < end && data[value] != ';' &&
                               out < (unsigned)cap - 1)
                            buf[out++] = data[value++];
                        while (out > 0 && (buf[out - 1] == ' ' ||
                                           buf[out - 1] == '\t')) out--;
                        buf[out] = 0;
                        return;
                    }
                }
            }
        }
        if (pos < size) pos++;
    }
}

void Lang_GetA(const char* section, const char* key, const char* def,
               char* buf, int cap)
{
    if (!buf || cap <= 0) return;
    buf[0] = 0;
    if (!section || !key)
    {
        Lang_CopyA(buf, cap, def);
        return;
    }
    Lang_EnsureInit();
    Lang_Read(s_Path, section, key, buf, cap);
    if (!buf[0]) Lang_Read(g_IniPath, section, key, buf, cap); /* 旧配置兼容 */
    if (!buf[0]) Lang_CopyA(buf, cap, def);
}

wchar_t* Lang_Utf8ToWide(wchar_t* dst, wchar_t* end, const unsigned char* s)
{
    unsigned c, c2;
    if (s[0] == 0xEF && s[1] == 0xBB && s[2] == 0xBF) s += 3;
    while (*s)
    {
        if (*s < 0x80)                  c = *s++;
        else if ((*s & 0xE0) == 0xC0)   { c = *s++ & 0x1Fu; c = (c << 6) | (*s++ & 0x3Fu); }
        else if ((*s & 0xF0) == 0xE0)
        {
            c = *s++ & 0x0Fu;
            c = (c << 6) | (*s++ & 0x3Fu);
            c = (c << 6) | (*s++ & 0x3Fu);
        }
        else if ((*s & 0xF8) == 0xF0)
        {
            c  = *s++ & 0x07u;
            c2 = *s++ & 0x3Fu;
            c  = (c << 6) | c2;
            c  = (c << 6) | (*s++ & 0x3Fu);
            c2 = *s++ & 0x3Fu;
            c  = (c << 6) | c2;
            c -= 0x10000u;
            if (dst + 1 < end) *dst++ = (wchar_t)(0xD800u + ((c >> 10) & 0x3FFu));
            if (dst < end)    *dst++ = (wchar_t)(0xDC00u + (c & 0x3FFu));
            continue;
        }
        else { s++; c = 0xFFFDu; }
        if (dst >= end) break;
        *dst++ = (wchar_t)c;
    }
    if (dst < end) *dst = 0;
    else end[-1] = 0;
    return dst;
}

int Lang_GetW(const char* section, const char* key, const wchar_t* def,
              wchar_t* buf, int cap)
{
    char tmp[LANG_VALUE_CAP];
    if (!buf || cap <= 0) return 0;
    buf[0] = 0;
    Lang_GetA(section, key, "", tmp, sizeof(tmp));
    if (tmp[0])
    {
        Lang_Utf8ToWide(buf, buf + cap, (const unsigned char*)tmp);
        if (buf[0]) return 1;
    }
    if (def)
    {
        int i = 0;
        while (i < cap - 1 && def[i]) { buf[i] = def[i]; i++; }
        buf[i] = 0;
    }
    return 0;
}
