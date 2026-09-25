#pragma one

template<typename K, typename V>
class OA_Map {
public:
    OA_Map();
    OA_Map(size_t capacity);
    void insert(const K& key, const V& value);
    void erase(const K& key);
    void count(const K& key);
    void empty();
    void clear();
    size_t size();
    V& operator[](const K& key);

private:
    size_t m_size = 0;
    size_t capacity;
    void rehash();
};

#include "oa_map.tpp"