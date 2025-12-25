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

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    StringHelperTests();

    std::printf("\033[32mAll tests passed.\033[0m\n");

    return EXIT_SUCCESS;
}