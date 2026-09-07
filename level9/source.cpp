#include <cstring>
#include <unistd.h>

class N
{
public:
    N(int value) : value(value)
    {
    }

    void setAnnotation(char *text)
    {
        std::memcpy(annotation, text, std::strlen(text));
    }

    virtual int operator+(N &other)
    {
        return value + other.value;
    }

    virtual int operator-(N &other)
    {
        return value - other.value;
    }

private:
    char annotation[100];
    int value;
};

int main(int argc, char **argv)
{
    N *left;
    N *right;

    if (argc <= 1)
        _exit(1);

    left = new N(5);
    right = new N(6);

    left->setAnnotation(argv[1]);
    return *right + *left;
}
