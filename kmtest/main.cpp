#include <erebus/erebuskm.hxx>

#include <erebus/util/exceptionutil.hxx>

#include <iomanip>
#include <iostream>


static int run(int argc, char* argv[])
{
    try
    {
        Erk::ErebusDriver erebus(0);

        auto taskCount = erebus.enumerateTasks(
            [](const Erk::ErebusDriver::Task& t)
            {
                std::cout << t.pid <<  " u=" << t.user << " k=" << t.system << "\n";
                return true;
            }
        );

        return EXIT_SUCCESS;
    }
    catch (Er::Exception& e)
    {
        std::cerr << Er::Util::formatException(e) << "\n";
    }
    catch (std::exception& e)
    {
        std::cerr << Er::Util::formatException(e) << "\n";
    }

    return EXIT_FAILURE;
}

int main(int argc, char* argv[])
{
    try
    {
        Er::LibScope er;

        return run(argc, argv);
    }
    catch (std::exception& e)
    {
        std::cerr << Er::Util::formatException(e) << "\n";
    }

    return EXIT_FAILURE;
}