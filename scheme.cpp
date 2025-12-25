bool StringEqual(const char* Str1, const char* Str2)
{
    while (*Str1 && *Str2)
    {
        if (*Str1 != *Str2)
            return false;
        ++Str1;
        ++Str2;
    }
    return *Str1 == *Str2;
}

struct string_builder {
    char* Start;
    char* Current;

    void Init(char* Buffer) {
        Start = Buffer;
        Current = Buffer;
        *Current = '\0';
    }

    void AppendChar(char c) {
        *Current++ = c;
        *Current = '\0';
    }

    void AppendString(const char* Str) {
        while (*Str) {
            AppendChar(*Str++);
        }
    }

    const char* GetString() const {
        return Start;
    }
};