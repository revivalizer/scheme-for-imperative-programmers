#include "scheme.cpp"

#include <cstdlib>
#include <cstdio>

void ExpectWithMessage(bool Condition, const char* Message)
{
    if (!Condition) {
        std::printf("\033[31mTest failed: %s\033[0m\n", Message);
        std::exit(EXIT_FAILURE);
    }

    std::printf("%s\n", Message);
}

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
    Parser.ParseSequenceUntil(&StringBuilder, '\0', &Error);

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
    Parser.ParseSequenceUntil(&StringBuilder, '\0', &Error);

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

#define Expect(CONDITION) ExpectWithMessage(CONDITION, #CONDITION)

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

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    StringHelperTests();
    BasicParseTests();
    ParseErrorTests();

    std::printf("\033[32mAll tests passed.\033[0m\n");

    return EXIT_SUCCESS;
}