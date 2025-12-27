void FATAL_ERROR(const char* Message);

int StringLength(const char* Str)
{
    int Length = 0;
    while (*Str++) Length++;
    return Length;
}

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
        Reset();
    }

    void Reset() {
        Current = Start;
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

#define PARSE_ERROR(ERROR, ROW, COL) { Error->Type = error::PARSE_ERROR; Error->Error = ERROR; Error->Row = ROW; Error->Col = COL; return; }
#define CHECK_ERROR() { if (Error->Type != error::NO_ERROR) return; }

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

    void ParseSExpressionSequenceUntil(string_builder* StringBuilder, char Delimiter, error* Error) {
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
                ParseSExpression(StringBuilder, Error); CHECK_ERROR();
                IndexCount++;
            }
        }
    }

    void ParseSExpression(string_builder* StringBuilder, error* Error) {
        if (Match('(')) {
            StringBuilder->Char('(');
            ParseSExpressionSequenceUntil(StringBuilder, ')', Error); CHECK_ERROR();
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

struct value {
    enum type {
        NIL,
        BOOLEAN,
        SYMBOL,
        PAIR,
    };

    struct pair {
        value* Car;
        value* Cdr;
    };

    bool IsAlive;
    type Type;
    union {
        bool Boolean;
        const char* Symbol;
        pair Pair;
    };

    static value Nil;
    static value True;
    static value False;
};

value value::Nil   = { true, value::NIL, { false } };
value value::True  = { true, value::BOOLEAN, { true } };
value value::False = { true, value::BOOLEAN, { false } };

struct mem {
    typedef void* (*alloc_func)(size_t NumBytes);

    value* ValuePool;
    int ValueCapacity;
    alloc_func AllocFunc;

    void Init(value* Pool, int Capacity, alloc_func Alloc) {
        ValuePool = Pool;
        ValueCapacity = Capacity;
        AllocFunc = Alloc;
    }

    value* AllocCell(value::type Type = value::NIL) {
        for (int i=0; i<ValueCapacity; i++) {
            if (ValuePool[i].IsAlive == false) {
                ValuePool[i] = {};
                ValuePool[i].Type = Type;
                ValuePool[i].IsAlive = true;
                return &ValuePool[i];
            }
        }
        FATAL_ERROR("AllocCellOOM");
        return nullptr;
    }

    value* AllocPair(value* Car, value* Cdr) {
        value* Cell = AllocCell(value::PAIR);
        Cell->Pair.Car = Car;
        Cell->Pair.Cdr = Cdr;
        return Cell;
    }

    value* AllocSymbol(const char* ZeroTerminatedSymbol) {
        return AllocSymbol(ZeroTerminatedSymbol, ZeroTerminatedSymbol + StringLength(ZeroTerminatedSymbol));
    }

    value* AllocSymbol(const char* SymbolStart, const char* SymbolEnd) {
        size_t Length = size_t(SymbolEnd - SymbolStart);
        char* Buffer = (char*)AllocFunc(Length + 1);
        for (size_t i=0; i<Length; i++) {
            Buffer[i] = SymbolStart[i];
        }
        Buffer[Length] = '\0';
        value* Symbol = AllocCell(value::SYMBOL);
        Symbol->Symbol = Buffer;
        return Symbol;
    }
};

void ValueToString(value* Value, string_builder* StringBuilder) {
    switch (Value->Type) {
        case value::NIL: {
            StringBuilder->String("()");
        } break;
        case value::BOOLEAN: {
            if (Value->Boolean) {
                StringBuilder->String("#t");
            } else {
                StringBuilder->String("#f");
            }
        } break;
        case value::SYMBOL: {
            StringBuilder->String(Value->Symbol);
        } break;
        case value::PAIR: {
            StringBuilder->Char('(');
            ValueToString(Value->Pair.Car, StringBuilder);
            value* Rest = Value->Pair.Cdr;
            while (Rest->Type == value::PAIR) {
                StringBuilder->Char(' ');
                ValueToString(Rest->Pair.Car, StringBuilder);
                Rest = Rest->Pair.Cdr;
            }
            if (Rest->Type != value::NIL) {
                StringBuilder->String(" . ");
                ValueToString(Rest, StringBuilder);
            }
            StringBuilder->Char(')');
        } break;
    }
}