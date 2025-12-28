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
    static const int ValuePoolCapacity = 100;
    value ValuePool[ValuePoolCapacity] = {};

    mem Mem = {};
    Mem.Init(ValuePool, ValuePoolCapacity, malloc); // NOTE: Pass in malloc because symbols are duplicated in AllocSymbol
    context Context = {};
    Context.Mem = &Mem;

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
    static const int ValuePoolCapacity = 100;
    value ValuePool[ValuePoolCapacity] = {};

    mem Mem = {};
    Mem.Init(ValuePool, ValuePoolCapacity, malloc); // NOTE: Pass in malloc because symbols are duplicated in AllocSymbol
    context Context = {};
    Context.Mem = &Mem;

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
        std::exit(EXIT_FAILURE);
    }
}

void ExpectEvalResult(const char* Input, const char* Expected) {
    static const int ValuePoolCapacity = 100;
    value ValuePool[ValuePoolCapacity] = {};

    mem Mem = {};
    Mem.Init(ValuePool, ValuePoolCapacity, malloc);
    context Context = {};
    Context.Mem = &Mem;

    error Error = {};

    parser Parser;
    Parser.Init(Input);
    value* ParseResult = Parser.ParseSExpression(&Context, &Error);
    HandleParseError(Input, &Error);

    value* EvalResult = eval::Eval(ParseResult, &Context, &Error);
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

void ExpectEvalError(const char* Input, const char* ExpectedError) {
    static const int ValuePoolCapacity = 100;
    value ValuePool[ValuePoolCapacity] = {};

    mem Mem = {};
    Mem.Init(ValuePool, ValuePoolCapacity, malloc);
    context Context = {};
    Context.Mem = &Mem;

    error Error = {};

    parser Parser;
    Parser.Init(Input);
    value* ParseResult = Parser.ParseSExpression(&Context, &Error);
    HandleParseError(Input, &Error);

    eval::Eval(ParseResult, &Context, &Error);

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
    static const int ValuePoolCapacity = 100;
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


    std::printf("\033[32mAll tests passed.\033[0m\n");

    return EXIT_SUCCESS;
}