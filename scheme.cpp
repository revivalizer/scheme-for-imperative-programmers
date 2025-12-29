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
        EVAL_ERROR,
    };
    type Type;
    const char* Error;
    int Row, Col;
    const char* Ex;
};

struct value; struct context;
typedef value* (*primitive_func_ptr)(value* Value, context* Context, error* Error);

struct value {
    enum type {
        NIL,
        BOOLEAN,
        SYMBOL,
        PAIR,
        PRIMITIVE_FUNCTION,
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
        primitive_func_ptr PrimitiveFunction;
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

    value* AllocPrimitiveFunction(primitive_func_ptr Func) {
        value* Cell = AllocCell(value::PRIMITIVE_FUNCTION);
        Cell->PrimitiveFunction = Func;
        return Cell;
    }
};

struct context {
    mem* Mem;
    value* Environment;
};

value* Cons(value* Car, value* Cdr, mem* Mem) {
    return Mem->AllocPair(Car, Cdr);
}

#define PARSE_ERROR(ERROR, ROW, COL) { Error->Type = error::PARSE_ERROR; Error->Error = ERROR; Error->Row = ROW; Error->Col = COL; return 0; }
#define CHECK_ERROR() { if (Error->Type != error::NO_ERROR) return 0; }

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

    value* ParseSExpressionSequenceUntil(char Delimiter, context* Context, error* Error, int ParenOpenRow = 0, int ParenOpenCol = 0) {
        while (true) {
            EatWhitespace();
            if (Match(Delimiter)) {
                return &value::Nil;
            } else if (Match('\0')) {
                PARSE_ERROR("UNMATCHED_OPEN_PAREN", ParenOpenRow, ParenOpenCol); // In practice we are only looking for open parens
            } else {
                value* Car = ParseSExpression(Context, Error); CHECK_ERROR();
                value* Cdr = ParseSExpressionSequenceUntil(Delimiter, Context, Error, ParenOpenRow, ParenOpenCol); CHECK_ERROR();
                return Context->Mem->AllocPair(Car, Cdr);
            }
        }
    }

    value* ParseSExpression(context* Context, error* Error) {
        if (Match('(')) {
            return ParseSExpressionSequenceUntil(')', Context, Error, Row, Col - 1);
        } else if (Match('#')) {
            if (Match('t')) {
                return &value::True;
            } else if (Match('f')) {
                return &value::False;;
            } else {
                PARSE_ERROR("UNEXPECTED_CHARACTER", Row, Col);
            }
        } else if (Match('\'')) {
            value* QuoteSymbol = Context->Mem->AllocSymbol("quote");
            value* QuotedExpr = ParseSExpression(Context, Error); CHECK_ERROR();
            return Cons(QuoteSymbol, Cons(QuotedExpr, &value::Nil, Context->Mem), Context->Mem);
        } else if (IsAllowableSymbolStartCharacter(C())) {
            const char* SymbolStart = Current;
            Next();
            while (IsAllowableSymbolCharacter(C())) {
                Next();
            }
            return Context->Mem->AllocSymbol(SymbolStart, Current);
        } else {
            PARSE_ERROR("UNEXPECTED_CHARACTER", Row, Col);
        }
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
        case value::PRIMITIVE_FUNCTION: {
            StringBuilder->String("#<primitive-function>");
        } break;
    }
}

#define EVAL_ERROR(ERROR) { Error->Type = error::EVAL_ERROR; Error->Error = ERROR; return 0; }
#define EVAL_ASSERT(CONDITION, ERROR) { if (!(CONDITION)) { EVAL_ERROR(ERROR); } }
#define EVAL_ASSERT_EX(CONDITION, ERROR, EX) { if (!(CONDITION)) { Error->Ex = EX; EVAL_ERROR(ERROR); } }

struct eval {
    static value* Car(value* Value) {
        return Value->Pair.Car;
    }

    static value* Cdr(value* Value) {
        return Value->Pair.Cdr;
    }

    static value* Cadr(value* Value) {
        return Car(Cdr(Value));
    }

    static int ListLength(value* List) {
        int Length = 0;
        while (List->Type == value::PAIR) {
            Length++;
            List = Cdr(List);
        }
        return Length;
    }

    static value* EvalList(value* List, context* Context, error* Error) {
        if (List->Type == value::NIL) {
            return &value::Nil;
        }

        return Cons(eval::Eval(Car(List), Context, Error), EvalList(Cdr(List), Context, Error), Context->Mem);
    }

    static bool NullQ(value* Value) {
        return Value->Type == value::NIL;
    }

    static bool NotNullQ(value* Value) {
        return !NullQ(Value);
    }

    static bool PairQ(value* Value) {
        return Value->Type == value::PAIR;
    }

    static bool ListQ(value* Value) {
        return PairQ(Value) || NullQ(Value);
    }

    static bool SymbolQ(value* Value) {
        return Value->Type == value::SYMBOL;
    }

    static bool BooleanQ(value* Value) {
        return Value->Type == value::BOOLEAN;
    }

    static bool ProcedureQ(value* Value) {
        return Value->Type == value::PRIMITIVE_FUNCTION;
    }

    static bool EqualQ(value* A, value* B) {
        if (A->Type != B->Type) return false;
        switch (A->Type) {
            case value::NIL: return true;
            case value::BOOLEAN: return A->Boolean == B->Boolean;
            case value::SYMBOL: return StringEqual(A->Symbol, B->Symbol);
            case value::PAIR: return EqualQ(Car(A), Car(B)) && EqualQ(Cdr(A), Cdr(B));
            case value::PRIMITIVE_FUNCTION: return A->PrimitiveFunction == B->PrimitiveFunction;
            default: return false;
        }
    }

    static value* Assoc(value* Needle, value* Haystack, error* Error) {
        // TODO: Could assert that Needle and Pair keys are symbols
        if (NullQ(Haystack)) {
            return &value::Nil;
        }

        value* Pair = Car(Haystack);
        EVAL_ASSERT(PairQ(Pair), "EVAL_ERROR_ASSOC_NON_PAIR_IN_HAYSTACK");

        value* Key = Car(Pair);
        if (StringEqual(Needle->Symbol, Key->Symbol)) {
            return Pair;
        }

        return Assoc(Needle, Cdr(Haystack), Error); // Don't need to check error here, since we are returning anyway
    }


    static value* EvalDefine(value* Operands, context* Context, error* Error) {
        EVAL_ASSERT(ListLength(Operands) == 2, "DEFINE_ARGUMENT_ERROR");
        value* Symbol = Car(Operands);
        value* Value = Eval(Cadr(Operands), Context, Error); CHECK_ERROR();
        value* Entry = Cons(Symbol, Value, Context->Mem);
        Context->Environment = Cons(Entry, Context->Environment, Context->Mem);
        return Symbol;
    }

    static value* Apply(value* Operator, value* Operands, context* Context, error* Error) {
        if (Operator->Type == value::PRIMITIVE_FUNCTION) {
            return Operator->PrimitiveFunction(Operands, Context, Error);
        }
        EVAL_ERROR("EVAL_ERROR_NOT_A_PROCEDURE");
    }

    static value* Eval(value* Expr, context* Context, error* Error) {
        (void)Context;
        switch (Expr->Type) {
            case value::NIL:
            case value::BOOLEAN:
                {
                    return Expr;
                } break;

            case value::SYMBOL:
                {
                    value* EnvCell = Assoc(Expr, Context->Environment, Error); CHECK_ERROR();
                    EVAL_ASSERT_EX(NotNullQ(EnvCell), "EVAL_UNDEFINED_SYMBOL", Expr->Symbol);
                    return Cdr(EnvCell);
                } break;

            case value::PAIR: {
                value* UnevaluatedOperator = Car(Expr);
                value* Operands = Cdr(Expr);

                if (UnevaluatedOperator->Type == value::SYMBOL) {
                    if (StringEqual(UnevaluatedOperator->Symbol, "quote")) {
                        EVAL_ASSERT(ListLength(Operands) == 1, "QUOTE_ARGUMENT_ERROR");
                        return Car(Operands);
                    } else if (StringEqual(UnevaluatedOperator->Symbol, "define")) {
                        return EvalDefine(Operands, Context, Error);
                    }
                }

                value* EvaluatedOperator = Eval(UnevaluatedOperator, Context, Error); CHECK_ERROR();
                value* EvaluatedOperands = EvalList(Operands, Context, Error); CHECK_ERROR();

                return Apply(EvaluatedOperator, EvaluatedOperands, Context, Error);
            }

            default: {
                EVAL_ERROR("UNHANDLED_EXPRESSION_TYPE");
            }
        }
    }

    static value* EvalSequence(value* Expr, context* Context, error* Error) {
        value* LastResult = &value::Nil;

        while (Expr->Type == value::PAIR) {
            LastResult = Eval(Car(Expr), Context, Error); CHECK_ERROR();
            Expr = Cdr(Expr);
        }
        return LastResult;
    }

    static value* CarFunc(value* Arguments, context* Context, error* Error) {
        (void)Context;
        EVAL_ASSERT(ListLength(Arguments) == 1, "CAR_ARGUMENT_ERROR");
        EVAL_ASSERT(PairQ(Car(Arguments)), "CAR_NON_PAIR_ARGUMENT");
        return Car(Car(Arguments));
    }

    static value* CdrFunc(value* Arguments, context* Context, error* Error) {
        (void)Context;
        EVAL_ASSERT(ListLength(Arguments) == 1, "CDR_ARGUMENT_ERROR");
        EVAL_ASSERT(PairQ(Car(Arguments)), "CDR_NON_PAIR_ARGUMENT");
        return Cdr(Car(Arguments));
    }

    static value* ConsFunc(value* Arguments, context* Context, error* Error) {
        EVAL_ASSERT(ListLength(Arguments) == 2, "CONS_ARGUMENT_ERROR");
        return Cons(Car(Arguments), Cadr(Arguments), Context->Mem);
    }

    static value* ListFunc(value* Arguments, context* Context, error* Error) {
        (void)Context;
        (void)Error;
        return Arguments;
    }

    static value* AssocFunc(value* Arguments, context* Context, error* Error) {
        (void)Context;
        EVAL_ASSERT(ListLength(Arguments) == 2, "ASSOC_ARGUMENT_ERROR");
        EVAL_ASSERT(ListQ(Cadr(Arguments)), "ASSOC_SECOND_ARGUMENT_MUST_BE_LIST");
        return Assoc(Car(Arguments), Cadr(Arguments), Error); CHECK_ERROR();
    }

    static value* PairQFunc(value* Arguments, context* Context, error* Error) {
        (void)Context; (void)Error;
        EVAL_ASSERT(ListLength(Arguments) == 1, "PAIR?_ARGUMENT_ERROR");
        return PairQ(Car(Arguments)) ? &value::True : &value::False;
    }

    static value* NullQFunc(value* Arguments, context* Context, error* Error) {
        (void)Context; (void)Error;
        EVAL_ASSERT(ListLength(Arguments) == 1, "NULL?_ARGUMENT_ERROR");
        return NullQ(Car(Arguments)) ? &value::True : &value::False;
    }

    static value* SymbolQFunc(value* Arguments, context* Context, error* Error) {
        (void)Context; (void)Error;
        EVAL_ASSERT(ListLength(Arguments) == 1, "SYMBOL?_ARGUMENT_ERROR");
        return SymbolQ(Car(Arguments)) ? &value::True : &value::False;
    }

    static value* BooleanQFunc(value* Arguments, context* Context, error* Error) {
        (void)Context; (void)Error;
        EVAL_ASSERT(ListLength(Arguments) == 1, "BOOLEAN?_ARGUMENT_ERROR");
        return BooleanQ(Car(Arguments)) ? &value::True : &value::False;
    }

    static value* ProcedureQFunc(value* Arguments, context* Context, error* Error) {
        (void)Context; (void)Error;
        EVAL_ASSERT(ListLength(Arguments) == 1, "PROCEDURE?_ARGUMENT_ERROR");
        return ProcedureQ(Car(Arguments)) ? &value::True : &value::False;
    }

    static value* EqualQFunc(value* Arguments, context* Context, error* Error) {
        (void)Context; (void)Error;
        EVAL_ASSERT(ListLength(Arguments) == 2, "EQUAL?_ARGUMENT_ERROR");
        return EqualQ(Car(Arguments), Cadr(Arguments)) ? &value::True : &value::False;
    }
};

value* ExtendEnvironmentWithPrimitiveFunction(value* Environment, const char* Name, primitive_func_ptr Func, mem* Mem) {
    value* Symbol = Mem->AllocSymbol(Name);
    value* Entry = Cons(Symbol, Mem->AllocPrimitiveFunction(Func), Mem);
    return Cons(Entry, Environment, Mem);
}

value* RegisterBuiltinFunctions(value* Environment, mem* Mem) {
    Environment = ExtendEnvironmentWithPrimitiveFunction(Environment, "car", &eval::CarFunc, Mem);
    Environment = ExtendEnvironmentWithPrimitiveFunction(Environment, "cdr", &eval::CdrFunc, Mem);
    Environment = ExtendEnvironmentWithPrimitiveFunction(Environment, "cons", &eval::ConsFunc, Mem);
    Environment = ExtendEnvironmentWithPrimitiveFunction(Environment, "list", &eval::ListFunc, Mem);
    Environment = ExtendEnvironmentWithPrimitiveFunction(Environment, "assoc", &eval::AssocFunc, Mem);
    Environment = ExtendEnvironmentWithPrimitiveFunction(Environment, "pair?", &eval::PairQFunc, Mem);
    Environment = ExtendEnvironmentWithPrimitiveFunction(Environment, "null?", &eval::NullQFunc, Mem);
    Environment = ExtendEnvironmentWithPrimitiveFunction(Environment, "symbol?", &eval::SymbolQFunc, Mem);
    Environment = ExtendEnvironmentWithPrimitiveFunction(Environment, "boolean?", &eval::BooleanQFunc, Mem);
    Environment = ExtendEnvironmentWithPrimitiveFunction(Environment, "procedure?", &eval::ProcedureQFunc, Mem);
    Environment = ExtendEnvironmentWithPrimitiveFunction(Environment, "equal?", &eval::EqualQFunc, Mem);
    return Environment;
}
