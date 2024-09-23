#include <span>
#include <locale.h>
#include "fqpv.hpp"

int main(int argc, const char** argv)
{
    setlocale(LC_ALL, "");

    fqpv::fqpv app;
    return app.main(std::span(argv, argc));
}
