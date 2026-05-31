#include <cstdint>
#include <cstddef>

#include <cstdlib>
#include <cstdio>

#include "scheme.cpp"

void FATAL_ERROR(const char* Message)
{
    std::printf("\033[31mFatal error: %s\033[0m\n", Message);
    std::exit(EXIT_FAILURE);
}

void ExpectWithMessage(bool Condition, const char* Message)
{
    if (!Condition) {
        std::printf("\033[31mTest failed: %s\033[0m\n", Message);
        std::exit(EXIT_FAILURE);
    }

    std::printf("%s\n", Message);
}

#define Expect(CONDITION) ExpectWithMessage(CONDITION, #CONDITION)

void HandleParseError(const char* Input, error* Error)
{
    if (Error->Type == error::PARSE_ERROR) {
        std::printf("\033[31mParse error for input \"%s\": \"%s\" at %d, %d\033[0m\n", Input, Error->Error, Error->Row, Error->Col);
        std::exit(EXIT_FAILURE);
    }
}

void ExpectParseResult(const char* Input, const char* Expected)
{
    // TODO: Consider calling CreateContext instead of stack allocating
    static const int ValuePoolCapacity = 300;
    value ValuePool[ValuePoolCapacity] = {};

    mem Mem = {};
    Mem.Init(ValuePool, ValuePoolCapacity, malloc); // NOTE: Pass in malloc because symbols are duplicated in AllocSymbol
    context Context = {};
    Context.Mem = &Mem;
    Context.Environment = Mem.AllocFrame(&value::Nil);

    error Error = {};

    parser Parser;
    Parser.Init(Input);
    value* Result = Parser.ParseSExpressionSequenceUntil('\0', &Context, &Error);
    Result = Result->Pair.Car; // ParseSExpressionSequenceUntil always returns a list

    HandleParseError(Input, &Error);

    char Buffer[1024];
    string_builder StringBuilder;
    StringBuilder.Init(Buffer);
    ValueToString(Result, &StringBuilder);

    const char* Actual = StringBuilder.Get();
    bool Equal = StringEqual(Actual, Expected);

    if (Equal) {
        std::printf("\"%s\" p-> \"%s\"\n", Input, Expected);
    } else {
        std::printf("\033[31m");
        std::printf("Test failed: \"%s\" p-> \"%s\"\n", Input, Expected);
        std::printf("Got: \"%s\"\n", Actual);
        std::printf("\033[0m");
        std::exit(EXIT_FAILURE);
    }
}

void ExpectParseError(const char* Input, const char* ErrorMessage, int Row, int Col)
{
    static const int ValuePoolCapacity = 300;
    value ValuePool[ValuePoolCapacity] = {};

    mem Mem = {};
    Mem.Init(ValuePool, ValuePoolCapacity, malloc); // NOTE: Pass in malloc because symbols are duplicated in AllocSymbol
    context Context = {};
    Context.Mem = &Mem;
    Context.Environment = Mem.AllocFrame(&value::Nil);

    error Error = {};

    parser Parser;
    Parser.Init(Input);
    Parser.ParseSExpressionSequenceUntil('\0', &Context, &Error);

    if (Error.Type != error::PARSE_ERROR) {
        std::printf("\033[31mExpected parse error but got none for input: \"%s\"\033[0m\n", Input);
        std::exit(EXIT_FAILURE);
    }

    if (!StringEqual(Error.Error, ErrorMessage) || Error.Row != Row || Error.Col != Col) {
        std::printf("\033[31mParse error did not match expected for input: \"%s\"\033[0m\n", Input);
        std::printf("Expected: \"%s\" at (%d, %d)\n", ErrorMessage, Row, Col);
        std::printf("Got: \"%s\" at (%d, %d)\n", Error.Error, Error.Row, Error.Col);
        std::exit(EXIT_FAILURE);
    }

    std::printf("\"%s\" p-> \"%s\" at %d, %d\n", Input, ErrorMessage, Row, Col);
}

void ExpectValueToStringResult(value* Value, const char* Expected) {
    char Buffer[256];
    string_builder StringBuilder;
    StringBuilder.Init(Buffer);

    ValueToString(Value, &StringBuilder);
    const char* Actual = StringBuilder.Get();
    bool Equal = StringEqual(Actual, Expected);

    if (Equal) {
        std::printf("ValueToString: \"%s\"\n", Actual);
    } else {
        std::printf("\033[31m");
        std::printf("ValueToString test failed: Expected \"%s\"\n", Expected);
        std::printf("Got: \"%s\"\n", Actual);
        std::printf("\033[0m");
        std::exit(EXIT_FAILURE);
    }
}

void HandleEvalError(const char* Input, error* Error) {
    if (Error->Type == error::EVAL_ERROR) {
        std::printf("\033[31mEval error for input \"%s\": \"%s\"\033[0m\n", Input, Error->Error);
        if (StringEqual(Error->Error, "EVAL_UNDEFINED_SYMBOL")) {
            std::printf("Undefined symbol: \"%s\"\n", Error->Ex);
        }
        std::exit(EXIT_FAILURE);
    }
}

void ExpectEvalResultWithContext(context* Context, const char* Input, const char* Expected) {
    error Error = {};

    parser Parser;
    Parser.Init(Input);
    value* ParseResult = Parser.ParseSExpressionSequenceUntil('\0', Context, &Error);
    HandleParseError(Input, &Error);

    value* EvalResult = eval::EvalSequence(ParseResult, Context, &Error);
    HandleEvalError(Input, &Error);

    char Buffer[1024];
    string_builder StringBuilder;
    StringBuilder.Init(Buffer);
    ValueToString(EvalResult, &StringBuilder);

    const char* Actual = StringBuilder.Get();
    bool Equal = StringEqual(Actual, Expected);

    if (Equal) {
        std::printf("\"%s\" e-> \"%s\"\n", Input, Expected);
    } else {
        std::printf("\033[31m");
        std::printf("Test failed: \"%s\" e-> \"%s\"\n", Input, Expected);
        std::printf("Got: \"%s\"\n", Actual);
        std::printf("\033[0m");
        std::exit(EXIT_FAILURE);
    }
}

void ExpectEvalResult(const char* Input, const char* Expected) {
    static const int ValuePoolCapacity = 300;
    value ValuePool[ValuePoolCapacity] = {};

    mem Mem = {};
    Mem.Init(ValuePool, ValuePoolCapacity, malloc);
    context Context = {};
    Context.Mem = &Mem;
    Context.Environment = Mem.AllocFrame(&value::Nil);
    RegisterBuiltinFunctions(Context.Environment, Context.Mem);

    ExpectEvalResultWithContext(&Context, Input, Expected);
}

void ExpectEvalErrorWithContext(context* Context, const char* Input, const char* ExpectedError) {
    error Error = {};

    parser Parser;
    Parser.Init(Input);
    value* ParseResult = Parser.ParseSExpressionSequenceUntil('\0', Context, &Error);
    HandleParseError(Input, &Error);

    eval::EvalSequence(ParseResult, Context, &Error);

    if (Error.Type != error::EVAL_ERROR) {
        std::printf("\033[31mExpected eval error but got none for input: \"%s\"\033[0m\n", Input);
        std::exit(EXIT_FAILURE);
    }

    if (!StringEqual(Error.Error, ExpectedError)) {
        std::printf("\033[31mEval error did not match expected for input: \"%s\"\033[0m\n", Input);
        std::printf("Expected: \"%s\"\n", ExpectedError);
        std::printf("Got: \"%s\"\n", Error.Error);
        std::exit(EXIT_FAILURE);
    }

    std::printf("\"%s\" e-> \"%s\"\n", Input, ExpectedError);
}

void ExpectEvalError(const char* Input, const char* ExpectedError) {
    static const int ValuePoolCapacity = 300;
    value ValuePool[ValuePoolCapacity] = {};

    mem Mem = {};
    Mem.Init(ValuePool, ValuePoolCapacity, malloc);
    context Context = {};
    Context.Mem = &Mem;
    Context.Environment = Mem.AllocFrame(&value::Nil);
    RegisterBuiltinFunctions(Context.Environment, Context.Mem);

    ExpectEvalErrorWithContext(&Context, Input, ExpectedError);
}

void StringHelperTests() {
    Expect(StringEqual("", "") == true);
    Expect(StringEqual("hello", "hello") == true);
    Expect(StringEqual("hello", "hell") == false);
    Expect(StringEqual("hello", "world") == false);

    char Buffer[256];
    string_builder Builder;
    Builder.Init(Buffer);

    Expect(StringEqual(Builder.Char('(').String("Hello ").String("world!").Char(')').Get(), "(Hello world!)"));
}

void BasicParseTests() {
    ExpectParseResult("  #t", "#t");
    ExpectParseResult("#f  ", "#f");

    ExpectParseResult("()", "()");
    ExpectParseResult(" (   ) ", "()");

    ExpectParseResult(" ( #t ) ", "(#t)");
    ExpectParseResult(" ( #t #f ) ", "(#t #f)");
    ExpectParseResult(" ( #t ( #f ) ) ", "(#t (#f))");
    ExpectParseResult(" ( #t ( #f (#t) ) ) ", "(#t (#f (#t)))");

    ExpectParseResult("a", "a");
    ExpectParseResult(".eww!@#$%^&*123'", ".eww!@#$%^&*123'");

    ExpectParseResult("(quote (a b c))", "(quote (a b c))");
    ExpectParseResult("(cdr (quote (a b c)))", "(cdr (quote (a b c)))");
    ExpectParseResult("(car (cdr (quote (a b c))))", "(car (cdr (quote (a b c))))");
}

void ParseErrorTests() {
    ExpectParseError("(", "UNMATCHED_OPEN_PAREN", 0, 0);
    ExpectParseError("( (a b c)", "UNMATCHED_OPEN_PAREN", 0, 0);
    ExpectParseError("( (a b c", "UNMATCHED_OPEN_PAREN", 0, 2);
    ExpectParseError("( \n (a b c", "UNMATCHED_OPEN_PAREN", 1, 1);
    ExpectParseError(")", "UNEXPECTED_CHARACTER", 0, 0);
    ExpectParseError("(#q)", "UNEXPECTED_CHARACTER", 0, 2);
}

void ValueTests() {
    value TestValue = {true, value::SYMBOL, { .Symbol = "test" }};

    // +-----+-----+
    // |     |     | -->   [nil]
    // +-----+-----+
    //   |
    // [#f]
    //
    value FalseListValue = {true, value::PAIR, { .Pair = { &value::False, &value::Nil }}};

    // +-----+-----+     +-----+-----+
    // |     |     | --> |     |     | -->   [nil]
    // +-----+-----+     +-----+-----+
    //   |                 |
    // [test]            [#f]
    //
    value TestFalseListValue = {true, value::PAIR, { .Pair = { &TestValue, &FalseListValue }}};


    // +-----+-----+     +-----+-----+     +-----+-----+
    // |     |     | --> |     |     | --> |     |     | -->   [nil]
    // +-----+-----+     +-----+-----+     +-----+-----+
    //   |                 |                 |
    // [#t]              [test]            [#f]
    //
    value TrueTestFalseListValue = {true, value::PAIR, { .Pair = { &value::True, &TestFalseListValue }}};

    // +-----+-----+     +-----+-----+
    // |     |     | --> |     |     | -->   [nil]
    // +-----+-----+     +-----+-----+
    //    |               |
    //    |              [#f]
    //    v
    // +-----+-----+     +-----+-----+     +-----+-----+
    // |     |     | --> |     |     | --> |     |     | -->   [nil]
    // +-----+-----+     +-----+-----+     +-----+-----+
    //   |                 |                 |
    // [#t]              [test]            [#f]
    //
    value ListInListValue = {true, value::PAIR, { .Pair = { &TrueTestFalseListValue, &FalseListValue }}};

    ExpectValueToStringResult(&value::False, "#f");
    ExpectValueToStringResult(&TestValue, "test");
    ExpectValueToStringResult(&FalseListValue, "(#f)");
    ExpectValueToStringResult(&TestFalseListValue, "(test #f)");
    ExpectValueToStringResult(&TrueTestFalseListValue, "(#t test #f)");
    ExpectValueToStringResult(&ListInListValue, "((#t test #f) #f)");
}

void ValuePoolAllocTests() {
    static const int ValuePoolCapacity = 300;
    value ValuePool[ValuePoolCapacity] = {};

    mem Mem = {};
    Mem.Init(ValuePool, ValuePoolCapacity, malloc); // NOTE: Pass in malloc because symbols are duplicated in AllocSymbol

    value* TestValue = Mem.AllocSymbol("test");
    value* FalseListValue = Mem.AllocPair(&value::False, &value::Nil);
    value* TestFalseListValue = Mem.AllocPair(TestValue, FalseListValue);
    value* TrueTestFalseListValue = Mem.AllocPair(&value::True, TestFalseListValue);
    value* ListInListValue = Mem.AllocPair(TrueTestFalseListValue, FalseListValue);

    ExpectValueToStringResult(&value::False, "#f");
    ExpectValueToStringResult(TestValue, "test");
    ExpectValueToStringResult(FalseListValue, "(#f)");
    ExpectValueToStringResult(TestFalseListValue, "(test #f)");
    ExpectValueToStringResult(TrueTestFalseListValue, "(#t test #f)");
    ExpectValueToStringResult(ListInListValue, "((#t test #f) #f)");
}

void EvalQuoteTests() {
    ExpectEvalResult("()", "()");
    ExpectEvalResult("#f", "#f");
    ExpectEvalResult("#t", "#t");
    ExpectEvalResult("(quote ())", "()");
    ExpectEvalResult("(quote a)", "a");
    ExpectEvalResult("(quote (a b c))", "(a b c)");

    ExpectEvalError("(quote)", "QUOTE_ARGUMENT_ERROR");
    ExpectEvalError("(quote a b)", "QUOTE_ARGUMENT_ERROR");
}

void EvalQuoteShorthandTests() {
    ExpectParseResult("'()", "(quote ())");
    ExpectParseResult("'a", "(quote a)");
    ExpectParseResult("'(a b c)", "(quote (a b c))");
    ExpectParseResult("''a", "(quote (quote a))");
}

void IfAndBeginTests() {
    ExpectEvalResult("(if #t 'yes 'no)", "yes");
    ExpectEvalResult("(if #f 'yes 'no)", "no");
    ExpectEvalResult("(if #t 'ok (quote))", "ok"); // (quote) would error if evaluated
    ExpectEvalResult("(if #f (quote) 'ok)", "ok");
    ExpectEvalError("(if)", "IF_ARGUMENT_ERROR");
    ExpectEvalResult("(begin 'a)", "a");
    ExpectEvalResult("(begin 'a 'b)", "b");
    ExpectEvalResult("(begin 'a (begin 'b 'c) 'd)", "d");
    ExpectEvalResult("(if #t (begin 'a 'b) 'no)", "b");
    ExpectEvalResult("(begin)", "()");
}

void AndOrTests() {
    ExpectEvalResult("(and)", "#t");
    ExpectEvalResult("(and 'a 'b 'c)", "c");
    ExpectEvalResult("(and #t 'hello)", "hello");
    ExpectEvalResult("(and #t #f (quote) 'a)", "#f");

    ExpectEvalResult("(or)", "#f");
    ExpectEvalResult("(or #f 'a (quote))", "a");
    ExpectEvalResult("(or #f #f 'x)", "x");
    ExpectEvalResult("(or #f #f #f)", "#f");
}

void ExtendableGlobalEnvironmentTests() {
    ExpectEvalResult("#t #f", "#f");

    ExpectEvalResult("(define test 'a) test", "a");
    ExpectEvalResult("(define test 'a) (define test2 'b) test", "a");
    ExpectEvalResult("(define test 'a) (define test2 'b) test2", "b");
    ExpectEvalResult("(define (test) #t 42) (test)", "42"); // Multiple expressions in body, should return last one

    ExpectEvalError("(define q)", "DEFINE_ARGUMENT_ERROR");
    ExpectEvalError("(define 1 'a)", "DEFINE_ARGUMENT_ERROR");
    ExpectEvalError("(define test 'a) test2", "EVAL_UNDEFINED_SYMBOL");
}

void PrimitiveProcedureTests() {
    ExpectEvalResult("(car '(a b c))", "a");
    ExpectEvalError("(car '(a b) '(b c))", "CAR_ARGUMENT_ERROR");
    ExpectEvalError("(car 'a)", "CAR_NON_PAIR_ARGUMENT");
    // NOTE: Error cases not exhaustively tested, feel free to add
    ExpectEvalResult("(cdr '(a b c))", "(b c)");
    ExpectEvalResult("(car (cdr '(a b c)))", "b");
    ExpectEvalResult("(cons 'a '(b c))", "(a b c)");
    ExpectEvalResult("(cons '(a b c) '())", "((a b c))");
    ExpectEvalResult("(car (cons '(a b c) #f))", "(a b c)");

    ExpectEvalResult("(list)", "()");
    ExpectEvalResult("(list 'a 'b 'c)", "(a b c)");
    ExpectEvalResult("(list (car (list 'c 'd 'e)) 'b 'a)", "(c b a)");

    ExpectEvalResult("(assoc 'joan '((john smith) (joan doe) (marcia law)))", "(joan doe)");
    ExpectEvalResult("(assoc 'john '((john smith) (joan doe) (marcia law)))", "(john smith)");
    ExpectEvalResult("(assoc 'jean '((john smith) (joan doe) (marcia law)))", "#f");
    ExpectEvalResult("(assoc 'a '())", "#f");
    ExpectEvalResult("(assoc 'b '((c d) (e f)))", "#f");
    ExpectEvalResult("(assoc 'c '((c d) (e f)))", "(c d)");
    ExpectEvalResult("(assoc 'e '((c d) (e f)))", "(e f)");
    ExpectEvalResult("(if (assoc 'a '((c d) (e f))) #t #f)", "#f");
    ExpectEvalResult("(if (assoc 'c '((c d) (e f))) #t #f)", "#t");

    ExpectEvalResult("(pair? '())", "#f");
    ExpectEvalResult("(pair? '#f)", "#f");
    ExpectEvalResult("(pair? (cdr '(a)))", "#f");
    ExpectEvalResult("(pair? '(a))", "#t");
    ExpectEvalResult("(pair? '(a b))", "#t");
    ExpectEvalResult("(pair? 'a)", "#f");
    // TODO: Add test for dotted pair later

    ExpectEvalResult("(null? '())", "#t");
    ExpectEvalResult("(null? '#f)", "#f");
    ExpectEvalResult("(null? (cdr '(a)))", "#t");
    ExpectEvalResult("(null? 'a)", "#f");
    ExpectEvalResult("(null? '(a b))", "#f");

    ExpectEvalResult("(symbol? 'symbol?)", "#t");
    ExpectEvalResult("(symbol? symbol?)", "#f");
    ExpectEvalResult("(symbol? 'a)", "#t");
    ExpectEvalResult("(symbol? '(a b))", "#f");
    ExpectEvalResult("(symbol? '())", "#f");
    ExpectEvalResult("(symbol? #t)", "#f");

    ExpectEvalResult("(boolean? #t)", "#t");
    ExpectEvalResult("(boolean? #f)", "#t");
    ExpectEvalResult("(boolean? 'true)", "#f");
    ExpectEvalResult("(boolean? '())", "#f");
    ExpectEvalResult("(boolean? '(#t))", "#f");

    ExpectEvalResult("(procedure? cons)", "#t");
    ExpectEvalResult("(procedure? procedure?)", "#t");
    ExpectEvalResult("(procedure? 'procedure?)", "#f");
    // TODO: Add tests for lambda later
}

void NumberTests() {
    ExpectParseResult("1", "1");
    ExpectParseResult("123", "123");
    ExpectParseResult("-12", "-12");
    ExpectParseResult("-0", "0");
    ExpectParseResult("(-4 2)", "(-4 2)");
    ExpectParseResult("(- 4 2)", "(- 4 2)");
    ExpectParseResult("100", "100");

    ExpectEvalResult("23", "23");

    ExpectEvalResult("(number? 2)", "#t");
    ExpectEvalResult("(number? 'a)", "#f");
    ExpectEvalResult("(number? #f)", "#f");

    ExpectEvalResult("(list 1 2 3)", "(1 2 3)");
    ExpectEvalResult("(assoc 971 '((442 smith) (971 doe) (887 law)))", "(971 doe)"); // Hint: Use equals to compare keys 
    ExpectEvalResult("(equal? 3 (+ 2 1))", "#t");
    ExpectEvalResult("(equal? 3 -8)", "#f");

    ExpectEvalResult("(+)", "0");
    ExpectEvalError("(+3)", "EVAL_UNDEFINED_SYMBOL"); // NOTE: +3 turns into a symbol, as opposed to -3 which turns into a number
    ExpectEvalResult("(+ 3)", "3");
    ExpectEvalResult("(+ 3 4 5)", "12");
    ExpectEvalResult("(+ 3 (+ 4 8) 5)", "20");

    ExpectEvalError("(-)", "SUB_ARGUMENT_ERROR");
    ExpectEvalError("(-1)", "EVAL_ERROR_NOT_A_PROCEDURE");
    ExpectEvalResult("(- 3)", "-3");
    ExpectEvalResult("(- 10 4)", "6");
    ExpectEvalResult("(- 10 4 1)", "5");

    ExpectEvalResult("(*)", "1");
    ExpectEvalResult("(* 3)", "3");
    ExpectEvalResult("(* 3 4 5)", "60");
    ExpectEvalResult("(* 3 (* 4 8) 5)", "480");

    ExpectEvalError("(/)", "DIV_ARGUMENT_ERROR");
    ExpectEvalError("(/ 2)", "DIV_ARGUMENT_ERROR");
    // NOTE: If you have floats, you may want to do this instead
    // ExpectEvalResult("(/ 1)", "1");
    // ExpectEvalResult("(/ 2)", "0.5");
    ExpectEvalResult("(/ 18 9)", "2");
    ExpectEvalResult("(/ 60 (/ 4 2) 5)", "6");
    ExpectEvalResult("(/ -5 1)", "-5");
    ExpectEvalError("(/ 1 0)", "DIVISION_BY_ZERO");

    ExpectEvalError("(<)", "LESS_THAN_ARGUMENT_ERROR");
    ExpectEvalError("(< 1)", "LESS_THAN_ARGUMENT_ERROR");
    ExpectEvalError("(< 1 2 3)", "LESS_THAN_ARGUMENT_ERROR");
    ExpectEvalError("(< 'a 'b)", "LESS_THAN_NON_NUMBER_ARGUMENT");
    ExpectEvalResult("(< 0 1)", "#t");
    ExpectEvalResult("(< 0 -1)", "#f");
    ExpectEvalResult("(< 10 10)", "#f");

    ExpectEvalError("(=)", "EQUALS_ARGUMENT_ERROR");
    ExpectEvalError("(= 1)", "EQUALS_ARGUMENT_ERROR");
    ExpectEvalError("(= 1 2 3)", "EQUALS_ARGUMENT_ERROR");
    ExpectEvalError("(= 'a 'b)", "EQUALS_NON_NUMBER_ARGUMENT");
    ExpectEvalResult("(= 0 1)", "#f");
    ExpectEvalResult("(= 3 3)", "#t");
}

void LetSetTests() {
    ExpectEvalResult("(let ((x 1)) x)", "1");
    ExpectEvalResult("(let ((x 1) (y 2)) (+ x y))", "3");
    ExpectEvalResult("(let () 42)", "42");
    ExpectEvalResult("(let ((x 1)) (let ((x 2)) x))", "2");
    ExpectEvalResult("(let ((x 1)) (let ((x 2)) x) x)", "1");
    ExpectEvalResult("(let ((x 1)) (set! x (+ x 1)) x)", "2");
    ExpectEvalError("(let 1 2)", "LET_ARGUMENT_ERROR");
    ExpectEvalError("(let ((x)) x)", "LET_ARGUMENT_ERROR");
    ExpectEvalError("(let (((1 2)) 3) 4)", "LET_ARGUMENT_ERROR");
    ExpectEvalError("(let ((x 1)))", "LET_ARGUMENT_ERROR");

    ExpectEvalResult("(let ((x 1)) (set! x 2) x)", "2");
    ExpectEvalResult("(let ((x 1)) (set! x 2) (set! x 3) x)", "3");
    ExpectEvalResult(
        "(let ((x 1)) "
        "  (let () (set! x 5)) "
        "  x)",
        "5"
    );
    ExpectEvalResult(
        "(let ((x 1) (y 10)) "
        "  (set! x (+ x y)) "
        "  x)",
        "11"
    );
    ExpectEvalResult(
        "(let ((x 1)) "
        "  (let ((x 2)) "
        "    (set! x 3) "
        "    x))",
        "3"
    );
    ExpectEvalResult(
        "(let ((x 1)) "
        "  (let ((x 2)) "
        "    (set! x 3)) "
        "  x)",
        "1"
    );
    ExpectEvalError("(set! x 1)", "SET!_UNBOUND_VARIABLE");
    ExpectEvalError("(begin (set! y 2) y)", "SET!_UNBOUND_VARIABLE");
    ExpectEvalError("(set!)", "SET!_ARGUMENT_ERROR");
    ExpectEvalError("(set! x)", "SET!_ARGUMENT_ERROR");
    ExpectEvalError("(set! x 1 2)", "SET!_ARGUMENT_ERROR");
    ExpectEvalError("(set! 1 2)", "SET!_ILLEGAL_TARGET");
    ExpectEvalError("(set! (quote x) 1)", "SET!_ILLEGAL_TARGET");
    // NOTE: You could think about adding these if it makes sense for your implementation
    // ExpectEvalError("(set! if 1)", "SET!_ILLEGAL_TARGET"); // Special form
    // ExpectEvalError("(set! + 1)", "SET!_ILLEGAL_TARGET"); // Registered function
}

void LambdaTests() {
    ExpectEvalResult("(procedure? (lambda (x) x))", "#t");

    ExpectEvalResult( "((lambda (x) x) 10)", "10");
    ExpectEvalResult( "((lambda (x y) (+ x y)) 1 2)", "3");
    ExpectEvalResult( "((lambda () 42))", "42");
    ExpectEvalResult( "(let ((add2 (lambda (x) (+ x 2)))) (add2 40))", "42");
    ExpectEvalResult(
        "(let ((x 1)) "               // outer x = 1
        "  (let ((f (lambda () x)))"  // f captures outer x
        "    (let ((x 2))"            // inner x = 2
        "      (f))))",               // should see 1
        "1"
    );
    ExpectEvalResult(
        "(let ((x 1)) "
        "  (let ((f (lambda () x))) "
        "    (set! x 2) "
        "    (f)))",
        "2"
    );
    ExpectEvalResult(
        "(let ((x 0)) "
        "  (let ((inc (lambda () (set! x (+ x 1)) x))) "
        "    (list (inc) (inc) (inc))) )",
        "(1 2 3)"
    );
    ExpectEvalResult(
        "(let ((x 10)) "
        "  (let ((f (lambda (y) (+ x y)))) "
        "    (f 5)))",
        "15"
    );
    ExpectEvalResult(
        "(let ((x 0)) "
        "  (let ((inc (lambda () (set! x (+ x 1)) x))) "
        "    (list (inc) (inc))))",
        "(1 2)"
    );
    ExpectEvalError( "((lambda (x) x))", "EVAL_ARGUMENT_LENGTH_MISMATCH");
    ExpectEvalError( "((lambda (x) x) 1 2)", "EVAL_ARGUMENT_LENGTH_MISMATCH");
    ExpectEvalError( "((let ((x 1)) x))", "EVAL_ERROR_NOT_A_PROCEDURE");
    ExpectEvalError( "((lambda 1 2) 3)", "LAMBDA_ARGUMENT_ERROR");
    ExpectEvalError( "(lambda (x))", "LAMBDA_ARGUMENT_ERROR");
}

void DottedPairTests() {
    ExpectParseResult("()", "()");
    ExpectParseResult("(a . b)", "(a . b)");
    ExpectParseResult("(1 . 2)", "(1 . 2)");
    ExpectParseResult("((a . b) . c)", "((a . b) . c)");
    ExpectParseResult("(a . (b . (c . ())))", "(a b c)"); // canonical print as proper list

    ExpectParseResult("(a b . c)", "(a b . c)");
    ExpectParseResult("(a b . (c))", "(a b c)");
    ExpectParseResult("(a b . (c d))", "(a b c d)");
    ExpectParseResult("(a . (b c))", "(a b c)");
    ExpectParseResult("(a . (b . c))", "(a b . c)");
    ExpectParseResult("(a b c . ())", "(a b c)");

    ExpectParseResult(".", ".");
    //ExpectParseResult("'(.)", "(quote (.))");    // list containing symbol "."
    //ExpectParseResult("(a .)", "(a .)");         // if you allow "." as symbol after a, not dotted syntax

    ExpectParseResult("'(a . b)", "(quote (a . b))");
    ExpectParseResult("'(a b . c)", "(quote (a b . c))");
    ExpectParseResult("'(a b . (c d))", "(quote (a b c d))");
    ExpectParseResult("'((a . b) c)", "(quote ((a . b) c))");

    // NOTE: These tests depend on your parser
    // ExpectParseResult("(a.b)", "(a . b)");
    // ExpectParseResult("(a .b)", "(a . b)");
    // ExpectParseResult("(a. b)", "(a . b)");
    ExpectParseResult("(a\t.\n b)", "(a . b)");

    ExpectParseError("( . a)", "ERROR_PARSE_DOT_IN_HEAD", 0, 3);
    ExpectParseError("(a .)", "ERROR_PARSE_DOT_MISSING_CDR", 0, 4);
    ExpectParseError("(a b .)", "ERROR_PARSE_DOT_MISSING_CDR", 0, 6);
    ExpectParseError("(a . b c)", "ERROR_PARSE_DOT_TOO_MANY_TAIL_ELEMENTS", 0, 7);
    ExpectParseError("(a . b . c)", "ERROR_PARSE_DOT_TOO_MANY_TAIL_ELEMENTS", 0, 7);
    ExpectParseError("(a b . c d)", "ERROR_PARSE_DOT_TOO_MANY_TAIL_ELEMENTS", 0, 9);

    //ExpectParseError("(a .. b)", "ERROR_PARSE_DOT_TOO_MANY_TAIL_ELEMENTS", 0, 0);
    ExpectParseError("(a . . b)", "ERROR_PARSE_DOT_TOO_MANY_TAIL_ELEMENTS", 0, 7);
}

void LambdaVariadicArgsTests() {
    ExpectEvalResult("((lambda args args))", "()");
    ExpectEvalResult("((lambda args args) 1)", "(1)");
    ExpectEvalResult("((lambda args args) 1 2 3)", "(1 2 3)");
    ExpectEvalResult("((lambda args (car args)) 10 20)", "10");
    ExpectEvalResult("((lambda (x . rest) x) 10)", "10");
    ExpectEvalResult("((lambda (x . rest) rest) 10)", "()");
    ExpectEvalResult("((lambda (x . rest) rest) 10 20 30)", "(20 30)");
    ExpectEvalResult("((lambda (x . rest) (car rest)) 10 20 30)", "20");
    ExpectEvalResult("((lambda (x y . rest) (list x y rest)) 1 2)", "(1 2 ())");
    ExpectEvalResult("((lambda (x y . rest) (list x y rest)) 1 2 3 4)", "(1 2 (3 4))");
    ExpectEvalResult("((lambda (x y . rest) rest) 1 2 3)", "(3)");
    ExpectEvalResult("((lambda (x . rest) (pair? rest)) 1 2 3)", "#t");
    ExpectEvalResult("((lambda (x . rest) (null? rest)) 1)", "#t");

    ExpectEvalError("((lambda (x . rest) x))", "EVAL_ARGUMENT_LENGTH_MISMATCH");
    ExpectEvalError("((lambda (x y . rest) x) 1)", "EVAL_ARGUMENT_LENGTH_MISMATCH");
    ExpectEvalError("((lambda (x y . rest) x))", "EVAL_ARGUMENT_LENGTH_MISMATCH");

    ExpectEvalError("((lambda (x . 123) x) 1)", "CLOSURE_INVALID_FORMALS");
    ExpectEvalError("((lambda ((x) . rest) x) 1)", "CLOSURE_INVALID_FORMALS");
}

void ApplyTests() {
    ExpectEvalResult("(apply (lambda (x) x) '(10))", "10");
    ExpectEvalResult("(apply + '(1 2 3 4))", "10");
    ExpectEvalResult("(apply + 1 2 '(3 4))", "10");
    ExpectEvalResult("(apply (lambda () 42) '())", "42");
    ExpectEvalResult("(let ((l '(1 2))) (apply + l))", "3");
    ExpectEvalResult(
        "(let ((f (lambda (a b c) (list a b c))))"
        "  (apply f '(1 2 3)))",
        "(1 2 3)");
    ExpectEvalResult(
        "(apply (lambda (f) (f 5))"
        "       (list (lambda (x) (* x 2))))",
        "10");
    ExpectEvalError("(apply 10 '(1 2 3))", "EVAL_ERROR_NOT_A_PROCEDURE");
    ExpectEvalError("(apply + 1 2 3)", "APPLY_ARGUMENT_ERROR");
    ExpectEvalError("(apply + '(1 . 2))", "APPLY_ARGUMENT_ERROR");
    ExpectEvalError("(apply + 10)", "APPLY_ARGUMENT_ERROR");
    ExpectEvalError("(apply +)", "APPLY_ARGUMENT_ERROR");
}

void DefineFunctionTests() {
    ExpectEvalResult("(define (id x) x) (id 'a)", "a");
    ExpectEvalError("(define (id x) x) (id)", "EVAL_ARGUMENT_LENGTH_MISMATCH");
    ExpectEvalError("(define (id x) x) (id 1 2)", "EVAL_ARGUMENT_LENGTH_MISMATCH");

    ExpectEvalResult("(define (id x) x) (define (use x) (id x)) (use 'z)", "z");
    ExpectEvalResult("(define (f x) x) (define (f x) (quote new)) (f 123)", "new");
    ExpectEvalResult("(define (f x) x) (procedure? f)", "#t");

    ExpectEvalResult("(define (f . args) args) (f 1 2 3)", "(1 2 3)");
    ExpectEvalResult("(define (f . args) args) (f)", "()");
    ExpectEvalResult("(define (g x . rest) rest) (g 10 20 30)", "(20 30)");
    ExpectEvalError("(define (g x . rest) rest) (g)", "EVAL_ARGUMENT_LENGTH_MISMATCH");

    ExpectEvalResult(
        "(define (fact n) "
        "  (if (= n 0) "
        "      1 "
        "      (* n (fact (- n 1))))) "
        "(fact 5)",
        "120"
    );

    ExpectEvalResult(
        "(define (fact-iter n acc) "
        "  (if (= n 0) "
        "      acc "
        "      (fact-iter (- n 1) (* acc n)))) "
        "(fact-iter 5 1)",
        "120"
    );

    ExpectEvalResult(
        "(define (even? n) (if (= n 0) #t (odd? (- n 1)))) "
        "(define (odd?  n) (if (= n 0) #f (even? (- n 1)))) "
        "(even? 6)",
        "#t"
    );

    ExpectEvalResult(
        "(define (f n) "
        "  (if (=  n 0) "
        "      0 "
        "      (begin (f (- n 1)) n))) "
        "(f 3)",
        "3"
    );
}

context* CreateContext(size_t ValuePoolCapacity) {
    value* ValuePool = (value*)calloc(ValuePoolCapacity, sizeof(value));

    mem* Mem = (mem*)calloc(sizeof(mem), 1);
    Mem->Init(ValuePool, (int)ValuePoolCapacity, malloc);
    context* Context = (context*)malloc(sizeof(context));
    Context->Mem = Mem;
    Context->Environment = Mem->AllocFrame(&value::Nil);
    RegisterBuiltinFunctions(Context->Environment, Context->Mem);
    return Context;
}

void GarbageCollect(context* Context) {
    Context->Mem->MarkAllInValuePoolDead();
    mem::MarkAliveRecursive(Context->Environment);
}

int NumAliveInValuePool(mem* Mem) {
    int Count = 0;
    for (int i = 0; i < Mem->ValuePoolCapacity; i++) {
        if (Mem->ValuePool[i].IsAlive) {
            Count++;
        }
    }
    return Count;
}

void MultipleInvocationWithGarbageCollectionTests() {
    const size_t DefaultValuePoolCapacity = 300;


    context* Context1 = CreateContext(DefaultValuePoolCapacity);
    ExpectEvalResultWithContext(Context1, "(define x 41)", "x");
    ExpectEvalResultWithContext(Context1, "x", "41");
    ExpectEvalResultWithContext(Context1, "(set! x 42) x", "42");
    ExpectEvalResultWithContext(Context1, "x", "42");


    context* Context2 = CreateContext(DefaultValuePoolCapacity);
    ExpectEvalErrorWithContext(Context2, "x", "EVAL_UNDEFINED_SYMBOL");


    context* Context3 = CreateContext(DefaultValuePoolCapacity);
    ExpectEvalResultWithContext(Context3, "(define a '(1 2 3))", "a");
    GarbageCollect(Context3);
    ExpectEvalResultWithContext(Context3, "a", "(1 2 3)");


    context* Context4 = CreateContext(DefaultValuePoolCapacity);
    ExpectEvalResultWithContext(
        Context4,
        "(define (make-adder x) (lambda (y) (+ x y))) "
        "(define add10 (make-adder 10))",
        "add10"
    );
    GarbageCollect(Context4);
    ExpectEvalResultWithContext(Context4, "(add10 5)", "15");


    context* Context5 = CreateContext(DefaultValuePoolCapacity);
    ExpectEvalResultWithContext(
        Context5,
        "(define (make-counter) "
        "  (begin "
        "    (define n 0) "
        "    (lambda () (begin (set! n (+ n 1)) n)))) "
        "(define c (make-counter)) ",
        "c"
    );
    GarbageCollect(Context5);
    ExpectEvalResultWithContext(Context5, "(c)", "1");
    GarbageCollect(Context5);
    ExpectEvalResultWithContext(Context5, "(c)", "2");


    context* Context6 = CreateContext(DefaultValuePoolCapacity);
    ExpectEvalResultWithContext(
        Context6,
        "(define (even? n) (if (= n 0) #t (odd? (- n 1)))) "
        "(define (odd?  n) (if (= n 0) #f (even? (- n 1)))) ",
        "odd?"
    );
    GarbageCollect(Context6);
    ExpectEvalResultWithContext(Context6, "(even? 10)", "#t");
    GarbageCollect(Context6);
    ExpectEvalResultWithContext(Context6, "(odd?  10)", "#f");


    context* Context7 = CreateContext(200); // You may have to tweak this number
    ExpectEvalResultWithContext(Context7, "(define z '(z))", "z");
    ExpectEvalResultWithContext(Context7, "(define tmp '())", "tmp");
    for (int i = 0; i < 50; i++) {
        ExpectEvalResultWithContext(
            Context7,
            "(set! tmp '(a b c d e f g h i j k l m n o p q r)) "
            "(set! tmp '()) ",
            "#<unspecified>"
        );
        GarbageCollect(Context7);
    }
    ExpectEvalResultWithContext(Context7, "z", "(z)");
}

void ListOperationAndFunctionalConceptTests() {
    const size_t DefaultValuePoolCapacity = 400;

    context* Context = CreateContext(DefaultValuePoolCapacity);
    ExpectEvalResultWithContext(Context,
        "(define (length xs) "
        "  (if (null? xs) 0 (+ 1 (length (cdr xs)))))",
        // "  1)", // TODO: Replace with actual implementation
        "length"
    );
    ExpectEvalResultWithContext(Context, "(length '())", "0");
    ExpectEvalResultWithContext(Context, "(length '(a b c d e))", "5");
    GarbageCollect(Context);

    // NOTE: append duplicates a (it has to), but not b
    ExpectEvalResultWithContext(Context,
        "(define (append a b) "
        "  (if (null? a) b (cons (car a) (append (cdr a) b))))",
        // "  '())", // TODO: Replace with actual implementation
        "append"
    );
    ExpectEvalResultWithContext(Context, "(append '() '())", "()");
    ExpectEvalResultWithContext(Context, "(append '(a) '())", "(a)");
    ExpectEvalResultWithContext(Context, "(append '() '(b))", "(b)");
    ExpectEvalResultWithContext(Context, "(append '(a b) '(c d))", "(a b c d)");
    GarbageCollect(Context);

    ExpectEvalResultWithContext(Context,
        "(define (reverse xs) "
        "  (if (null? xs) '() (append (reverse (cdr xs)) (list (car xs)))))",
        // "  '())", // TODO: Replace with actual implementation
        "reverse"
    );
    ExpectEvalResultWithContext(Context, "(reverse '())", "()");
    ExpectEvalResultWithContext(Context, "(reverse '(a b c d))", "(d c b a)");
    GarbageCollect(Context);

    ExpectEvalResultWithContext(Context,
        "(define (map f xs) "
        "  (if (null? xs) '() "
        "    (cons (f (car xs)) (map f (cdr xs)))))",
        // "  '())", // TODO: Replace with actual implementation
        "map"
    );
    ExpectEvalResultWithContext(Context, "(map (lambda (x) (* x 2)) '(1 2 3))", "(2 4 6)");
    ExpectEvalResultWithContext(Context, "(map car '((a b) (2 3) (e f)))", "(a 2 e)");
    GarbageCollect(Context);

}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    StringHelperTests();
    BasicParseTests();
    ParseErrorTests();
    ValueTests();
    ValuePoolAllocTests();
    EvalQuoteTests();
    EvalQuoteShorthandTests();
    IfAndBeginTests();
    AndOrTests();
    ExtendableGlobalEnvironmentTests();
    PrimitiveProcedureTests();
    NumberTests();
    LetSetTests();
    LambdaTests();
    DottedPairTests();
    LambdaVariadicArgsTests();
    ApplyTests();
    DefineFunctionTests();
    MultipleInvocationWithGarbageCollectionTests();
    ListOperationAndFunctionalConceptTests();

    std::printf("\033[32mAll tests passed.\033[0m\n");

    return EXIT_SUCCESS;
}