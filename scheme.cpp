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

    string_builder& Char(char c) {
        *Current++ = c;
        *Current = '\0';
        return *this;
    }

    string_builder& String(const char* Str) {
        while (*Str) {
            Char(*Str++);
        }
        return *this;
    }

    const char* Get() const {
        return Start;
    }
};

struct error {
    enum type {
        NO_ERROR = 0,
        PARSE_ERROR,
    };
    type Type;
    const char* Error;
    int Row, Col;
};

#define CHECK_ERROR() { if (Error->Type != error::NO_ERROR) return; }
#define PARSE_ERROR(ERROR, ROW, COL) { Error->Type = error::PARSE_ERROR; Error->Error = ERROR; Error->Row = ROW; Error->Col = COL; return; }

struct parser {
	const char* Current;
    int Col, Row;

    void Init(const char* Start) {
		Current = Start;
		Col = 0;
		Row = 0;
	}

    char C() {
        return Current[0];
    }

    void Next() {
        Current++;
        Col++;
    }

    bool Match(char Char) {
        if (C() == Char) {
            Next();
            return true;
        }
        return false;
    }

    static bool IsWhitespace(char C) {
        return C == ' ' || C == '\t' || C == '\n' || C == '\r';
    }

    static bool IsAllowableSymbolCharacter(char C) {
        return !(IsWhitespace(C) || C == '(' || C == ')' || C == '\0');
    }

    static bool IsAllowableSymbolStartCharacter(char C) {
        return IsAllowableSymbolCharacter(C);
    }

    void EatWhitespace() {
        while (IsWhitespace(C()) || C() == ';') {
            if (C() == '\n') {
                Next();
                Row++;
                Col = 0;
            } else {
                Next();
            }
        }
    }

    void ParseSequenceUntil(string_builder* StringBuilder, char Delimiter, error* Error) {
        int IndexCount = 0;

        int StartCol = Col - 1;
        int StartRow = Row;

        while (true) {
            EatWhitespace();
            if (Match(Delimiter)) {
                return;
            } else if (Match('\0')) {
                PARSE_ERROR("UNMATCHED_OPEN_PAREN", StartRow, StartCol); // In practice we are only looking for open parens
            } else {
                if (IndexCount > 0) {
                    StringBuilder->Char(' ');
                }
                Parse(StringBuilder, Error); CHECK_ERROR();
                IndexCount++;
            }
        }
    }

    void Parse(string_builder* StringBuilder, error* Error) {
        if (Match('(')) {
            StringBuilder->Char('(');
            ParseSequenceUntil(StringBuilder, ')', Error); CHECK_ERROR();
            StringBuilder->Char(')');
        } else if (Match('#')) {
            if (Match('t')) {
                StringBuilder->String("#t");
            } else if (Match('f')) {
                StringBuilder->String("#f");
            } else {
                PARSE_ERROR("UNEXPECTED_CHARACTER", Row, Col);
            }
        } else if (IsAllowableSymbolStartCharacter(C())) {
            StringBuilder->Char(C());
            Next();
            while (IsAllowableSymbolCharacter(C())) {
                StringBuilder->Char(C());
                Next();
            }
        } else {
            PARSE_ERROR("UNEXPECTED_CHARACTER", Row, Col);
        }
    }
};