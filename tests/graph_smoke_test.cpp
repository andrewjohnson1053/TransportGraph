#include <filesystem>
#include <iostream>

int main()
{
    const std::filesystem::path sampleMap{"assets/maps/sample_city.json"};
    if (!std::filesystem::is_regular_file(sampleMap))
    {
        std::cerr << "Missing sample map asset: " << sampleMap << '\n';
        return 1;
    }

    return 0;
}