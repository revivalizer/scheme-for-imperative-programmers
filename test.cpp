#include <cstdint>
#include <cstddef>

#include "scheme.cpp"

#include <cstdlib>
#include <cstdio>

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
    char Buffer[1024];
    string_builder StringBuilder;
    StringBuilder.Init(Buffer);

    error Error = {};

    parser Parser;
    Parser.Init(Input);
    Parser.ParseSExpressionSequenceUntil(&StringBuilder, '\0', &Error);

    HandleParseError(Input, &Error);

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
    char Buffer[1024];
    string_builder StringBuilder;
    StringBuilder.Init(Buffer);

    error Error = {};

    parser Parser;
    Parser.Init(Input);
    Parser.ParseSExpressionSequenceUntil(&StringBuilder, '\0', &Error);

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
    ExpectParseResult("() ()", "() ()");
    ExpectParseResult(" (   ) ", "()");

    ExpectParseResult(" ( #t ) ", "(#t)");
    ExpectParseResult(" ( #t #f ) ", "(#t #f)");
    ExpectParseResult(" ( #t #f )  ( #t #f ) ", "(#t #f) (#t #f)");
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

static const int ValuePoolCapacity = 100;
value ValuePool[ValuePoolCapacity];

void ValueTests() {
    value TestValue = {value::SYMBOL, { .Symbol = "test" }};

    // +-----+-----+
    // |     |     | -->   [nil]
    // +-----+-----+
    //   |
    // [#f]
    //
    value FalseListValue = {value::PAIR, { .Pair = { &value::False, &value::Nil }}};

    // +-----+-----+     +-----+-----+
    // |     |     | --> |     |     | -->   [nil]
    // +-----+-----+     +-----+-----+
    //   |                 |
    // [test]            [#f]
    //
    value TestFalseListValue = { value::PAIR, { .Pair = { &TestValue, &FalseListValue }}};


    // +-----+-----+     +-----+-----+     +-----+-----+
    // |     |     | --> |     |     | --> |     |     | -->   [nil]
    // +-----+-----+     +-----+-----+     +-----+-----+
    //   |                 |                 |
    // [#t]              [test]            [#f]
    //
    value TrueTestFalseListValue = { value::PAIR, { .Pair = { &value::True, &TestFalseListValue }}};

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
    value ListInListValue = { value::PAIR,{ .Pair = { &TrueTestFalseListValue, &FalseListValue }}};

    ExpectValueToStringResult(&value::False, "#f");
    ExpectValueToStringResult(&TestValue, "test");
    ExpectValueToStringResult(&FalseListValue, "(#f)");
    ExpectValueToStringResult(&TestFalseListValue, "(test #f)");
    ExpectValueToStringResult(&TrueTestFalseListValue, "(#t test #f)");
    ExpectValueToStringResult(&ListInListValue, "((#t test #f) #f)");
}

// void ValueTests() {
//     mem Mem;
//     Mem.Init(ValuePool, ValuePoolCapacity, malloc);

//     value* TestValue = Mem.AllocSymbol("test");

//     // +-----+-----+
//     // | car | cdr | -->   [nil]
//     // +-----+-----+
//     //   |
//     // [#f]
//     value* FalseListValue = Mem.AllocPair(&value::False, &value::Nil);

//     // +-----+-----+     +-----+-----+
//     // | car | cdr | --> | car | cdr | -->   [nil]
//     // +-----+-----+     +-----+-----+
//     //   |                 |
//     // [test]            [#f]
//     value* TestFalseListValue = Mem.AllocPair(TestValue, FalseListValue);


//     // +-----+-----+     +-----+-----+     +-----+-----+
//     // | car | cdr | --> | car | cdr | --> | car | cdr | -->   [nil]
//     // +-----+-----+     +-----+-----+     +-----+-----+
//     //   |                 |                 |
//     // [#t]              [test]            [#f]
//     value* TrueTestFalseListValue = Mem.AllocPair(&value::True, TestFalseListValue);

//     // +-----+-----+     +-----+-----+
//     // | car | cdr | --> | car | cdr | -->   [nil]
//     // +-----+-----+     +-----+-----+
//     //    |               |
//     //    |              [#f]
//     //    v
//     // +-----+-----+     +-----+-----+     +-----+-----+
//     // | car | cdr | --> | car | cdr | --> | car | cdr | -->   [nil]
//     // +-----+-----+     +-----+-----+     +-----+-----+
//     //   |                 |                 |
//     // [#t]              [test]            [#f]
//     value* ListInListValue = Mem.AllocPair(TrueTestFalseListValue, FalseListValue);

//     ExpectValueToStringResult(&value::False, "#f");
//     ExpectValueToStringResult(TestValue, "test");
//     ExpectValueToStringResult(FalseListValue, "(#f)");
//     ExpectValueToStringResult(TestFalseListValue, "(test #f)");
//     ExpectValueToStringResult(TrueTestFalseListValue, "(#t test #f)");
//     ExpectValueToStringResult(ListInListValue, "((#t test #f) #f)");
// }

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    StringHelperTests();
    BasicParseTests();
    ParseErrorTests();
    ValueTests();

    std::printf("\033[32mAll tests passed.\033[0m\n");

    return EXIT_SUCCESS;
}