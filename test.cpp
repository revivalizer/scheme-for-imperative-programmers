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

void ExpectParseResult(const char* Input, const char* Expected)
{
    char Buffer[1024];
    string_builder StringBuilder;
    StringBuilder.Init(Buffer);

    parser Parser;
    Parser.Init(Input);
    Parser.ParseSequenceUntil(&StringBuilder, '\0');

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

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    StringHelperTests();
    BasicParseTests();

    std::printf("\033[32mAll tests passed.\033[0m\n");

    return EXIT_SUCCESS;
}