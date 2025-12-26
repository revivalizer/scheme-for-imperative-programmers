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

    void EatWhitespace() {
        while (IsWhitespace(C()) || C() == ';') {
            if (C() == '\n') {
                Row++;
                Col = 0;
                Next();
            } else {
                Next();
            }
        }
    }

    // NOTE: Change to bool return false if no match
    void ParseSequenceUntil(string_builder* StringBuilder, char Delimiter) {
        int IndexCount = 0;

        while (true) {
            EatWhitespace();
            if (Match(Delimiter)) {
                return;
            } else {
                if (IndexCount > 0) {
                    StringBuilder->Char(' ');
                }
                Parse(StringBuilder);
                IndexCount++;
            }
        }
    }

    void Parse(string_builder* StringBuilder) {
        if (Match('(')) {
            StringBuilder->Char('(');
            ParseSequenceUntil(StringBuilder, ')');
            StringBuilder->Char(')');
        } else if (Match('#')) {
            if (Match('t')) {
                StringBuilder->String("#t");
            } else if (Match('f')) {
                StringBuilder->String("#f");
            }
        } else {
            while (IsWhitespace(C())==false && C()!=0 && C()!='(' && C()!=')') {
                StringBuilder->Char(C());
                Next();
            }
        }
    }
};