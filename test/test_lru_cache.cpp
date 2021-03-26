#include "lru_cache.hpp"
#include "lru_cache_custom.hpp"
#include "unordered_map.hpp"

int main()
{
    {
        lru::Cache<std::string, int> lru_cache(3, 0);
        lru_cache.insert("one", 1);
        lru_cache.insert("two", 2);
        lru_cache.insert("three", 3);
        lru_cache.insert("four", 4);
        std::cout << "\n==== lru::Cache ====" << std::endl;
        std::cout << lru_cache.contains("one") << std::endl;
        std::cout << lru_cache.contains("two") << std::endl;
        std::cout << lru_cache.contains("three") << std::endl;
        std::cout << lru_cache.contains("four") << std::endl;
    }
    {
        lru_custom::Cache<
            std::string, int,
            xxw::unordered_map<std::string, lru_custom::Node<std::string, int> *>>
            lru_cache(3, 0);
        lru_cache.insert("one", 1);
        lru_cache.insert("two", 2);
        lru_cache.insert("three", 3);
        lru_cache.insert("four", 4);
        std::cout << "\n==== lru_custom::Cache ====" << std::endl;
        std::cout << lru_cache.contains("one") << std::endl;
        std::cout << lru_cache.contains("two") << std::endl;
        std::cout << lru_cache.contains("three") << std::endl;
        std::cout << lru_cache.contains("four") << std::endl;
    }
    return 0;
}