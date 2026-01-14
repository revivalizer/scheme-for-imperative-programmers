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
        UNSPECIFIED,
        NIL,
        BOOLEAN,
        NUMBER,
        SYMBOL,
        PAIR,
        PRIMITIVE_PROCEDURE,
        COMPOUND_PROCEDURE,
        FRAME,
    };

    struct pair {
        value* Car;
        value* Cdr;
    };

    struct frame {
        value* Parent;
        value* Bindings;
    };

    bool IsAlive;
    type Type;
    union {
        bool Boolean;
        int64_t Number;
        const char* Symbol;
        pair Pair;
        primitive_func_ptr PrimitiveProcedure;
        frame Frame;
    };

    static value Unspecified;
    static value Nil;
    static value True;
    static value False;
};

value value::Unspecified   = { true, value::UNSPECIFIED, { false } };
value value::Nil           = { true, value::NIL, { false } };
value value::True          = { true, value::BOOLEAN, { true } };
value value::False     = { true, value::BOOLEAN, { false } };

struct mem {
    typedef void* (*alloc_func)(size_t NumBytes);

    value* ValuePool;
    int ValuePoolCapacity;
    alloc_func AllocFunc;

    void Init(value* Pool, int Capacity, alloc_func Alloc) {
        ValuePool = Pool;
        ValuePoolCapacity = Capacity;
        AllocFunc = Alloc;
    }

    value* AllocValue(value::type Type = value::NIL) {
        for (int i=0; i<ValuePoolCapacity; i++) {
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
        value* Value = AllocValue(value::PAIR);
        Value->Pair.Car = Car;
        Value->Pair.Cdr = Cdr;
        return Value;
    }

    value* AllocSymbol(const char* ZeroTerminatedSymbol) {
        return AllocSymbol(ZeroTerminatedSymbol, ZeroTerminatedSymbol + StringLength(ZeroTerminatedSymbol));
    }

    value* AllocNumber(int64_t Number) {
        value* Value = AllocValue(value::NUMBER);
        Value->Number = Number;
        return Value;
    }

    value* AllocSymbol(const char* SymbolStart, const char* SymbolEnd) {
        size_t Length = size_t(SymbolEnd - SymbolStart);
        char* Buffer = (char*)AllocFunc(Length + 1);
        for (size_t i=0; i<Length; i++) {
            Buffer[i] = SymbolStart[i];
        }
        Buffer[Length] = '\0';
        value* Value = AllocValue(value::SYMBOL);
        Value->Symbol = Buffer;
        return Value;
    }

    value* AllocPrimitiveProcedure(primitive_func_ptr Proc) {
        value* Value = AllocValue(value::PRIMITIVE_PROCEDURE);
        Value->PrimitiveProcedure = Proc;
        return Value;
    }

    value* AllocFrame(value* Parent) {
        value* Value = AllocValue(value::FRAME);
        Value->Frame.Parent = Parent;
        Value->Frame.Bindings = &value::Nil;
        return Value;
    }

    void MarkAllInValuePoolDead() {
        for (int i=0; i<ValuePoolCapacity; i++) {
            ValuePool[i].IsAlive = false;
        }
    }

    static void MarkAliveRecursive(value* Value) {
        if (Value->IsAlive) {
            return;
        }

        Value->IsAlive = true;

        switch (Value->Type){
            case value::COMPOUND_PROCEDURE:
            case value::PAIR:
            {
                MarkAliveRecursive(Value->Pair.Car);
                MarkAliveRecursive(Value->Pair.Cdr);
            } break;
            case value::FRAME: {
                MarkAliveRecursive(Value->Frame.Parent);
                MarkAliveRecursive(Value->Frame.Bindings);
            } break;
            default: {
                // No action needed
            } break;
        }
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
#define PARSE_ASSERT(CONDITION, ERROR, ROW, COL) { if (!(CONDITION)) { PARSE_ERROR(ERROR, ROW, COL); } }
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

    char NextC() {
        return Current[1];
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

    static bool IsDigit(char C) {
        return C >= '0' && C <= '9';
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
        bool FirstElement = (Row == ParenOpenRow) && ((Col - 1) == ParenOpenCol);
        while (true) {
            EatWhitespace();
            if (Match(Delimiter)) {
                return &value::Nil;
            } else if (Match('\0')) {
                PARSE_ERROR("UNMATCHED_OPEN_PAREN", ParenOpenRow, ParenOpenCol); // In practice we are only looking for open parens
            } else {
                if (Delimiter == ')' && Match('.')) {
                    PARSE_ASSERT(!FirstElement, "ERROR_PARSE_DOT_IN_HEAD", Row, Col);

                    EatWhitespace();
                    PARSE_ASSERT(C() != ')', "ERROR_PARSE_DOT_MISSING_CDR", Row, Col);

                    value* Last = ParseSExpression(Context, Error); CHECK_ERROR();

                    EatWhitespace();
                    if (!Match(')')) {
                        PARSE_ERROR("ERROR_PARSE_DOT_TOO_MANY_TAIL_ELEMENTS", Row, Col);
                    }

                    return Last;
                }

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
        } else if (IsDigit(C()) || (C() == '-' && IsDigit(NextC()))) {
            bool IsNegative = Match('-');
            int64_t Number = 0;
            while (IsDigit(C())) {
                Number = Number * 10 + (C() - '0');
                Next();
            }
            if (IsNegative) {
                Number = -Number;
            }
            return Context->Mem->AllocNumber(Number);
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
        case value::UNSPECIFIED: {
            StringBuilder->String("#<unspecified>");
        } break;
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
        case value::NUMBER: {
            int64_t Number = Value->Number;

            if (Number < 0) {
                StringBuilder->Char('-');
                Number = -Number;
            }

            if (Number == 0) {
                StringBuilder->Char('0');
                return;
            }

            char Chars[15];
            int NumChars = 0;

            while (Number > 0) {
                char Digit = (char)(Number % 10);
                Chars[NumChars++] = '0' + Digit;
                Number /= 10;
            }

            for (int i = NumChars - 1; i >= 0; i--) {
                StringBuilder->Char(Chars[i]);
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
        case value::PRIMITIVE_PROCEDURE: {
            StringBuilder->String("#<primitive-procedure>");
        } break;
        case value::COMPOUND_PROCEDURE: {
            StringBuilder->String("#<compound-procedure>");
        } break;
        case value::FRAME: {
            FATAL_ERROR("FRAME_TO_STRING");
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

    static value* Caddr(value* Value) {
        return Car(Cdr(Cdr(Value)));
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

    static bool ProperListQ(value* List) {
        while (List->Type == value::PAIR) {
            List = Cdr(List);
        }
        return NullQ(List);
    }

    static bool SymbolQ(value* Value) {
        return Value->Type == value::SYMBOL;
    }

    static bool BooleanQ(value* Value) {
        return Value->Type == value::BOOLEAN;
    }

    static bool NumberQ(value* Value) {
        return Value->Type == value::NUMBER;
    }

    static bool ProcedureQ(value* Value) {
        return Value->Type == value::PRIMITIVE_PROCEDURE || Value->Type == value::COMPOUND_PROCEDURE;
    }

    static bool FalseQ(value* Value) {
        return Value->Type == value::BOOLEAN && Value->Boolean == false;
    }

    static bool TrueQ(value* Value) {
        return !FalseQ(Value);
    }

    static bool EqualQ(value* A, value* B) {
        if (A->Type != B->Type) return false;
        switch (A->Type) {
            case value::UNSPECIFIED: return false; // ??
            case value::NIL: return true;
            case value::BOOLEAN: return A->Boolean == B->Boolean;
            case value::NUMBER: return A->Number == B->Number;
            case value::SYMBOL: return StringEqual(A->Symbol, B->Symbol);
            case value::PAIR: return EqualQ(Car(A), Car(B)) && EqualQ(Cdr(A), Cdr(B));
            case value::PRIMITIVE_PROCEDURE: return A->PrimitiveProcedure == B->PrimitiveProcedure;
            case value::COMPOUND_PROCEDURE: return A == B;
            case value::FRAME: {
                FATAL_ERROR("FRAME_EQUAL");
                return false;
            } break;
        }
    }

    static value* Assoc(value* Needle, value* Haystack, error* Error) {
        if (NullQ(Haystack)) {
            return &value::Nil;
        }

        value* Pair = Car(Haystack);
        EVAL_ASSERT(PairQ(Pair), "EVAL_ERROR_ASSOC_NON_PAIR_IN_HAYSTACK");

        value* Key = Car(Pair);
        if (EqualQ(Needle, Key)) {
            return Pair;
        }

        return Assoc(Needle, Cdr(Haystack), Error);
    }

    static void ExtendEnvironment(value* Name, value* Value, value* Environment, mem* Mem) {
        value* Entry = Cons(Name, Value, Mem);
        Environment->Frame.Bindings = Cons(Entry, Environment->Frame.Bindings, Mem);
    }

    static void ExtendEnvironmentWithLists(value* Names, value* Values, value* Environment, mem* Mem) {
        while (PairQ(Names)) {
            ExtendEnvironment(Car(Names), Car(Values), Environment, Mem);
            Names = Cdr(Names);
            Values = Cdr(Values);
        }
    }

    static value* EvalDefine(value* Operands, context* Context, error* Error) {
        EVAL_ASSERT(ListLength(Operands) == 2, "DEFINE_ARGUMENT_ERROR");
        if (PairQ(Car(Operands))) {
            value* NameAndArgs = Car(Operands);
            value* Body = Cdr(Operands);

            value* Name = Car(NameAndArgs);
            value* Args = Cdr(NameAndArgs);

            value* Lambda = EvalLambda(Args, Body, Context, Error); CHECK_ERROR();
            ExtendEnvironment(Name, Lambda, Context->Environment, Context->Mem);

            return Name;
        } else {
            value* Symbol = Car(Operands);
            EVAL_ASSERT(SymbolQ(Symbol), "DEFINE_ARGUMENT_ERROR");
            value* Value = Eval(Cadr(Operands), Context, Error); CHECK_ERROR();
            ExtendEnvironment(Symbol, Value, Context->Environment, Context->Mem);
            return Symbol;
        }
    }

    static value* EvalIf(value* Operands, context* Context, error* Error) {
        EVAL_ASSERT(ListLength(Operands) == 3, "IF_ARGUMENT_ERROR");
        value* Test = Car(Operands);
        value* Consequent = Cadr(Operands);
        value* Alternative = Caddr(Operands);

        value* TestResult = Eval(Test, Context, Error); CHECK_ERROR();
        return TrueQ(TestResult) ? Eval(Consequent, Context, Error) : Eval(Alternative, Context, Error);
    }

    static value* EvalAnd(value* Operands, context* Context, error* Error) {
        value* Result = &value::True;
        while (Operands->Type == value::PAIR) {
            Result = Eval(Car(Operands), Context, Error); CHECK_ERROR();
            if (FalseQ(Result)) {
                return &value::False;
            }
            Operands = Cdr(Operands);
        }
        return Result;
    }

    static value* EvalOr(value* Operands, context* Context, error* Error) {
        while (Operands->Type == value::PAIR) {
            value* Result = Eval(Car(Operands), Context, Error); CHECK_ERROR();
            if (TrueQ(Result)) {
                return Result;
            }
            Operands = Cdr(Operands);
        }
        return &value::False;
    }

    static value* EvalLet(value* Operands, context* Context, error* Error) {
        EVAL_ASSERT(ListLength(Operands) >= 2, "LET_ARGUMENT_ERROR");
        value* Bindings = Car(Operands);
        EVAL_ASSERT(ListQ(Bindings), "LET_ARGUMENT_ERROR");
        value* Body = Cdr(Operands);

        value* NewEnvironment = Context->Mem->AllocFrame(Context->Environment);

        while (Bindings->Type == value::PAIR) {
            value* Binding = Car(Bindings);
            EVAL_ASSERT(ListLength(Binding) == 2, "LET_ARGUMENT_ERROR");
            value* Name = Car(Binding);
            EVAL_ASSERT(SymbolQ(Name), "LET_ARGUMENT_ERROR");
            value* Value = Eval(Cadr(Binding), Context, Error); CHECK_ERROR();
            ExtendEnvironment(Name, Value, NewEnvironment, Context->Mem);
            Bindings = Cdr(Bindings);
        }

        context LetContext = *Context;
        LetContext.Environment = NewEnvironment;
        return EvalSequence(Body, &LetContext, Error);
    }

    static value* EvalSetBang(value* Operands, context* Context, error* Error) {
        EVAL_ASSERT(ListLength(Operands) == 2, "SET!_ARGUMENT_ERROR");
        value* Name = Car(Operands);
        EVAL_ASSERT(SymbolQ(Name), "SET!_ILLEGAL_TARGET");
        value* EnvCell = Lookup(Name, Context->Environment, Error); CHECK_ERROR();
        EVAL_ASSERT_EX(NotNullQ(EnvCell), "SET!_UNBOUND_VARIABLE", Name->Symbol);
        value* Value = Eval(Cadr(Operands), Context, Error); CHECK_ERROR();
        EnvCell->Pair.Cdr = Value;
        return &value::Unspecified;
    }

    static value* MakeClosure(value* Arguments, value* BodySequence, value* Environment, mem* Mem) {
        value* Procedure = Cons(BodySequence, Environment, Mem);
        Procedure = Cons(Arguments, Procedure, Mem);
        Procedure->Type = value::COMPOUND_PROCEDURE;
        return Procedure;
    }

    static value* EvalLambda(value* Arguments, value* BodySequence, context* Context, error* Error) {
        EVAL_ASSERT(ListQ(Arguments) || SymbolQ(Arguments), "LAMBDA_ARGUMENT_ERROR");
        EVAL_ASSERT(ListLength(BodySequence) > 0, "LAMBDA_ARGUMENT_ERROR");
        value* ParentEnvironment = Context->Environment;
        value* Closure = MakeClosure(Arguments, BodySequence, ParentEnvironment, Context->Mem);
        return Closure;
    }

    static value* EvalApplyArgsRec(value* Args, context* Context, error* Error) {
        EVAL_ASSERT(ListQ(Args), "APPLY_ARGUMENT_ERROR");
        if (NullQ(Cdr(Args))) {
            EVAL_ASSERT(ListQ(Car(Args)), "APPLY_ARGUMENT_ERROR");
            value* EvaluatedList = Eval(Car(Args), Context, Error); CHECK_ERROR();
            EVAL_ASSERT(ProperListQ(EvaluatedList), "APPLY_ARGUMENT_ERROR");
            return EvaluatedList;
        } else {
            return Cons(Eval(Car(Args), Context, Error), EvalApplyArgsRec(Cdr(Args), Context, Error), Context->Mem);
        }
    }

    static value* EvalApply(value* Operands, context* Context, error* Error) {
        EVAL_ASSERT(ListLength(Operands) >= 2, "APPLY_ARGUMENT_ERROR");
        value* Operator = Eval(Car(Operands), Context, Error); CHECK_ERROR();
        value* EvaluatedArgs = EvalApplyArgsRec(Cdr(Operands), Context, Error); CHECK_ERROR();
        return Apply(Operator, EvaluatedArgs, Context, Error);
    }

    static value* Apply(value* Operator, value* Operands, context* Context, error* Error) {
        if (Operator->Type == value::PRIMITIVE_PROCEDURE) {
            return Operator->PrimitiveProcedure(Operands, Context, Error);
        } else if (Operator->Type == value::COMPOUND_PROCEDURE) {
            value* Arguments = Car(Operator);
            value* BodySequence = Car(Cdr(Operator));
            value* ParentEnvironment = Cdr(Cdr(Operator));
            value* NewEnvironment = Context->Mem->AllocFrame(ParentEnvironment);

            if (SymbolQ(Arguments)) {
                ExtendEnvironment(Arguments, Operands, NewEnvironment, Context->Mem);
            } else {
                while (PairQ(Arguments)) {
                    EVAL_ASSERT(SymbolQ(Car(Arguments)), "CLOSURE_INVALID_FORMALS");
                    EVAL_ASSERT(PairQ(Operands), "EVAL_ARGUMENT_LENGTH_MISMATCH");

                    ExtendEnvironment(Car(Arguments), Car(Operands), NewEnvironment, Context->Mem);
                    Arguments = Cdr(Arguments);
                    Operands = Cdr(Operands);
                }
                if (!NullQ(Arguments)) {
                    EVAL_ASSERT(SymbolQ(Arguments), "CLOSURE_INVALID_FORMALS");
                    ExtendEnvironment(Arguments, Operands, NewEnvironment, Context->Mem);
                } else {
                    EVAL_ASSERT(NullQ(Operands), "EVAL_ARGUMENT_LENGTH_MISMATCH");
                }
            }
            context ApplyContext = *Context;
            ApplyContext.Environment = NewEnvironment;
            return EvalSequence(BodySequence, &ApplyContext, Error);
        }
        EVAL_ERROR("EVAL_ERROR_NOT_A_PROCEDURE");
    }

    static value* Lookup(value* Expr, value* Environment, error* Error) {
        value* FrameValue = Assoc(Expr, Environment->Frame.Bindings, Error); CHECK_ERROR();
        if (NotNullQ(FrameValue)) {
            return FrameValue;
        }
        if (NullQ(Environment->Frame.Parent)) {
            return &value::Nil;
        }
        return Lookup(Expr, Environment->Frame.Parent, Error);
    }

    static value* Eval(value* Expr, context* Context, error* Error) {
        (void)Context;
        switch (Expr->Type) {
            case value::FRAME: {
                FATAL_ERROR("FRAME_EVAL");
                return nullptr;
            } break;

            case value::UNSPECIFIED:
            case value::NIL:
            case value::BOOLEAN:
            case value::NUMBER:
            case value::PRIMITIVE_PROCEDURE:
            case value::COMPOUND_PROCEDURE:
                {
                    return Expr;
                } break;

            case value::SYMBOL:
                {
                    value* EnvCell = Lookup(Expr, Context->Environment, Error); CHECK_ERROR();
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
                    } else if (StringEqual(UnevaluatedOperator->Symbol, "if")) {
                        return EvalIf(Operands, Context, Error);
                    } else if (StringEqual(UnevaluatedOperator->Symbol, "begin")) {
                        return EvalSequence(Operands, Context, Error);
                    } else if (StringEqual(UnevaluatedOperator->Symbol, "and")) {
                        return EvalAnd(Operands, Context, Error);
                    } else if (StringEqual(UnevaluatedOperator->Symbol, "or")) {
                        return EvalOr(Operands, Context, Error);
                    } else if (StringEqual(UnevaluatedOperator->Symbol, "let")) {
                        return EvalLet(Operands, Context, Error);
                    } else if (StringEqual(UnevaluatedOperator->Symbol, "set!")) {
                        return EvalSetBang(Operands, Context, Error);
                    } else if (StringEqual(UnevaluatedOperator->Symbol, "lambda")) {
                        value* Arguments = Car(Operands);
                        value* BodySequence = Cdr(Operands);
                        return EvalLambda(Arguments, BodySequence, Context, Error);
                    } else if (StringEqual(UnevaluatedOperator->Symbol, "apply")) {
                        return EvalApply(Operands, Context, Error);
                    }
                }

                value* EvaluatedOperator = Eval(UnevaluatedOperator, Context, Error); CHECK_ERROR();
                value* EvaluatedOperands = EvalList(Operands, Context, Error); CHECK_ERROR();

                return Apply(EvaluatedOperator, EvaluatedOperands, Context, Error);
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

    static value* NumberQFunc(value* Arguments, context* Context, error* Error) {
        (void)Context; (void)Error;
        EVAL_ASSERT(ListLength(Arguments) == 1, "NUMBER?_ARGUMENT_ERROR");
        return NumberQ(Car(Arguments)) ? &value::True : &value::False;
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

    static value* AddFunc(value* Arguments, context* Context, error* Error) {
        (void)Context;
        int64_t Sum = 0;
        while (Arguments->Type == value::PAIR) {
            EVAL_ASSERT(NumberQ(Car(Arguments)), "ADD_NON_NUMBER_ARGUMENT");
            Sum += Car(Arguments)->Number;
            Arguments = Cdr(Arguments);
        }
        return Context->Mem->AllocNumber(Sum);
    }

    static value* SubFunc(value* Arguments, context* Context, error* Error) {
        (void)Context;
        EVAL_ASSERT(NotNullQ(Arguments), "SUB_ARGUMENT_ERROR");
        EVAL_ASSERT(NumberQ(Car(Arguments)), "SUB_NON_NUMBER_ARGUMENT");
        int64_t Result = Car(Arguments)->Number;
        Arguments = Cdr(Arguments);
        if (Arguments->Type == value::NIL) {
            Result = -Result;
        } else {
            while (Arguments->Type == value::PAIR) {
                EVAL_ASSERT(NumberQ(Car(Arguments)), "SUB_NON_NUMBER_ARGUMENT");
                Result -= Car(Arguments)->Number;
                Arguments = Cdr(Arguments);
            }
        }
        return Context->Mem->AllocNumber(Result);
    }

    static value* MulFunc(value* Arguments, context* Context, error* Error) {
        (void)Context;
        int64_t Product = 1;
        while (Arguments->Type == value::PAIR) {
            EVAL_ASSERT(NumberQ(Car(Arguments)), "MUL_NON_NUMBER_ARGUMENT");
            Product *= Car(Arguments)->Number;
            Arguments = Cdr(Arguments);
        }
        return Context->Mem->AllocNumber(Product);
    }

    static value* DivFunc(value* Arguments, context* Context, error* Error) {
        (void)Context;
        EVAL_ASSERT(NotNullQ(Arguments), "DIV_ARGUMENT_ERROR");
        EVAL_ASSERT(NumberQ(Car(Arguments)), "DIV_NON_NUMBER_ARGUMENT");
        int64_t Result = Car(Arguments)->Number;
        Arguments = Cdr(Arguments);
        EVAL_ASSERT(NotNullQ(Arguments), "DIV_ARGUMENT_ERROR");
        while (Arguments->Type == value::PAIR) {
            EVAL_ASSERT(NumberQ(Car(Arguments)), "DIV_NON_NUMBER_ARGUMENT");
            int64_t Divisor = Car(Arguments)->Number;
            EVAL_ASSERT(Divisor != 0, "DIVISION_BY_ZERO");
            Result /= Divisor;
            Arguments = Cdr(Arguments);
        }
        return Context->Mem->AllocNumber(Result);
    }

    static value* LessThanFunc(value* Arguments, context* Context, error* Error) {
        (void)Context;
        EVAL_ASSERT(ListLength(Arguments) == 2, "LESS_THAN_ARGUMENT_ERROR");
        EVAL_ASSERT(NumberQ(Car(Arguments)), "LESS_THAN_NON_NUMBER_ARGUMENT");
        EVAL_ASSERT(NumberQ(Cadr(Arguments)), "LESS_THAN_NON_NUMBER_ARGUMENT");
        return (Car(Arguments)->Number < Cadr(Arguments)->Number) ? &value::True : &value::False;
    }

    static value* EqualsFunc(value* Arguments, context* Context, error* Error) {
        (void)Context;
        EVAL_ASSERT(ListLength(Arguments) == 2, "EQUALS_ARGUMENT_ERROR");
        EVAL_ASSERT(NumberQ(Car(Arguments)), "EQUALS_NON_NUMBER_ARGUMENT");
        EVAL_ASSERT(NumberQ(Cadr(Arguments)), "EQUALS_NON_NUMBER_ARGUMENT");
        return (Car(Arguments)->Number == Cadr(Arguments)->Number) ? &value::True : &value::False;
    }
};

void ExtendEnvironmentWithPrimitiveProcedure(value* Environment, const char* Name, primitive_func_ptr Proc, mem* Mem) {
    value* Symbol = Mem->AllocSymbol(Name);
    value* Entry = Cons(Symbol, Mem->AllocPrimitiveProcedure(Proc), Mem);
    Environment->Frame.Bindings = Cons(Entry, Environment->Frame.Bindings, Mem);
}

void RegisterBuiltinFunctions(value* Environment, mem* Mem) {
    ExtendEnvironmentWithPrimitiveProcedure(Environment, "car", &eval::CarFunc, Mem);
    ExtendEnvironmentWithPrimitiveProcedure(Environment, "cdr", &eval::CdrFunc, Mem);
    ExtendEnvironmentWithPrimitiveProcedure(Environment, "cons", &eval::ConsFunc, Mem);
    ExtendEnvironmentWithPrimitiveProcedure(Environment, "list", &eval::ListFunc, Mem);
    ExtendEnvironmentWithPrimitiveProcedure(Environment, "assoc", &eval::AssocFunc, Mem);
    ExtendEnvironmentWithPrimitiveProcedure(Environment, "pair?", &eval::PairQFunc, Mem);
    ExtendEnvironmentWithPrimitiveProcedure(Environment, "null?", &eval::NullQFunc, Mem);
    ExtendEnvironmentWithPrimitiveProcedure(Environment, "symbol?", &eval::SymbolQFunc, Mem);
    ExtendEnvironmentWithPrimitiveProcedure(Environment, "boolean?", &eval::BooleanQFunc, Mem);
    ExtendEnvironmentWithPrimitiveProcedure(Environment, "number?", &eval::NumberQFunc, Mem);
    ExtendEnvironmentWithPrimitiveProcedure(Environment, "procedure?", &eval::ProcedureQFunc, Mem);
    ExtendEnvironmentWithPrimitiveProcedure(Environment, "equal?", &eval::EqualQFunc, Mem);

    ExtendEnvironmentWithPrimitiveProcedure(Environment, "+", &eval::AddFunc, Mem);
    ExtendEnvironmentWithPrimitiveProcedure(Environment, "-", &eval::SubFunc, Mem);
    ExtendEnvironmentWithPrimitiveProcedure(Environment, "*", &eval::MulFunc, Mem);
    ExtendEnvironmentWithPrimitiveProcedure(Environment, "/", &eval::DivFunc, Mem);
    ExtendEnvironmentWithPrimitiveProcedure(Environment, "<", &eval::LessThanFunc, Mem);
    ExtendEnvironmentWithPrimitiveProcedure(Environment, "=", &eval::EqualsFunc, Mem);
}
