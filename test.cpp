#include "scheme.cpp"

#include <cstdlib>
#include <cstdio>

void Expect2(bool Condition, const char* ConditionString)
{
    if (!Condition) {
        std::printf("\033[31mTest failed: %s\033[0m\n", ConditionString);
        std::exit(EXIT_FAILURE);
    }

    std::printf("%s\n", ConditionString);
}

#define Expect(CONDITION) Expect2(CONDITION, #CONDITION)

void StringHelperTests() {
    Expect(StringEqual("", "") == true);
    Expect(StringEqual("hello", "hello") == true);
    Expect(StringEqual("hello", "hell") == false);
    Expect(StringEqual("hello", "world") == false);

    char Buffer[256];
    string_builder Builder;
    Builder.Init(Buffer);

    Builder.AppendChar('(');
    Builder.AppendString("Hello ");
    Builder.AppendString("world!");
    Builder.AppendChar(')');
    Expect(StringEqual(Builder.GetString(), "(Hello world!)"));
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    StringHelperTests();

    std::printf("\033[32mAll tests passed.\033[0m\n");

    return EXIT_SUCCESS;
}