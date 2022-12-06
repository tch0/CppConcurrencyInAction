#include <iostream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <future>
#include <shared_mutex>
#include <chrono>
#include <utility>
#include <algorithm>
#include <list>
#include <vector>
#include <map>
#include <string>
#include <cassert>
using namespace std::chrono_literals;

template<typename Key, typename Value, typename Hash = std::hash<Key>>
class ThreadSafeLookupTable
{
private:
    class BucketType
    {
        friend class ThreadSafeLookupTable<Key, Value, Hash>;
    private:
        using BucketValue = std::pair<Key, Value>;
        using BucketData = std::list<BucketValue>;
        using BucketIterator = typename BucketData::iterator;
        BucketData m_data;
        mutable std::shared_mutex m_mutex;
        auto findEntryFor(const Key& key) const
        {
            return std::find_if(m_data.begin(), m_data.end(), [&](const BucketValue& item) -> bool { return item.first == key; });
        }
        auto findEntryFor(const Key& key)
        {
            return std::find_if(m_data.begin(), m_data.end(), [&](const BucketValue& item) -> bool { return item.first == key; });
        }
    public:
        Value valueFor(const Key& key, const Value& defaultValue) const
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex); // lock on shared mode.
            auto foundEntry = findEntryFor(key);
            return (foundEntry == m_data.end()) ? defaultValue : foundEntry->second;
        }
        void addOrUpdateMapping(const Key& key, const Value& value)
        {
            std::lock_guard<std::shared_mutex> lock(m_mutex);
            auto foundEntry = findEntryFor(key);
            if (foundEntry == m_data.end()) // not found, insert
            {
                m_data.emplace_back(key, value);
            }
            else // found, modify
            {
                foundEntry->second = value;
            }
        }
        void removeMapping(const Key& key)
        {
            std::lock_guard<std::shared_mutex> lock(m_mutex);
            auto foundEntry = findEntryFor(key);
            if (foundEntry != m_data.end()) // found
            {
                m_data.erase(foundEntry);
            }
        }
    };
private:
    std::vector<std::unique_ptr<BucketType>> m_buckets;
    Hash m_hasher;
    BucketType& getBucket(const Key& key) const
    {
        const std::size_t bucketIndex = m_hasher(key) % m_buckets.size();
        return *m_buckets[bucketIndex];
    }
public:
    using KeyType = Key;
    using MappedType = Value;
    using HashType = Hash;
    ThreadSafeLookupTable(std::size_t numBuckets = 19, const Hash& hasher = Hash()) // numBuckets better be prime number
        : m_buckets(numBuckets)
        , m_hasher(hasher)
    {
        for (std::size_t i = 0; i < numBuckets; ++i)
        {
            m_buckets[i].reset(new BucketType());
        }
    }
    ThreadSafeLookupTable(const ThreadSafeLookupTable&) = delete;
    ThreadSafeLookupTable& operator=(const ThreadSafeLookupTable&) = delete;
    Value valueFor(const Key& key, const Value& defaultValue = Value()) const
    {
        return getBucket(key).valueFor(key, defaultValue);
    }
    void addOrUpdateMapping(const Key& key, const Value& value)
    {
        getBucket(key).addOrUpdateMapping(key, value);
    }
    void removeMapping(const Key& key)
    {
        getBucket(key).removeMapping(key);
    }
    // get a snapshot of lookup table
    std::map<Key, Value> getMap() const
    {
        std::vector<std::unique_lock<std::shared_mutex>> locks;
        locks.reserve(m_buckets.size());
        for (std::size_t i = 0; i < m_buckets.size(); ++i)
        {
            locks.emplace_back(m_buckets[i].m_mutex);
        }
        std::map<Key, Value> res;
        for (std::size_t i = 0; i < m_buckets.size(); ++i)
        {
            for (auto iter = m_buckets[i].data.begin(); iter != m_buckets[i].data.end(); ++iter)
            {
                res.insert(*iter);
            }
        }
        return res;
    }
};

int main(int argc, char const *argv[])
{
    ThreadSafeLookupTable<int, std::string> table(103);
    for (int i = 0; i < 100; ++i)
    {
        table.addOrUpdateMapping(i, std::to_string(i));
        table.addOrUpdateMapping(i, std::to_string(i) + "_copy");
        std::this_thread::sleep_for(1ms);
    }
    auto lookupFunc = [&table](int count) {
        for (int i = 0; i < 10; ++i)
        {
            std::string value = table.valueFor(count * 10 + i);
            assert(value == std::to_string(count * 10 + i) + "_copy");
            std::this_thread::sleep_for(5ms);
        }
    };
    {
        std::vector<std::jthread> vec;
        for (int i = 0; i < 10; ++i)
        {
            vec.emplace_back(lookupFunc, i);
        }
    }
    return 0;
}
