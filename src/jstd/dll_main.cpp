
//#include "jstd/hashmap/cluster_flat_map.hpp"

#include <utility>
#include <type_traits>

#include "jstd/traits/type_traits.h"

template <typename K, typename V>
union map_slot_type {
public:
    using key_type = typename std::remove_const<K>::type;
    using mapped_type = typename std::remove_const<V>::type;
    using value_type = std::pair<const key_type, mapped_type>;
    using init_type = std::pair<key_type, mapped_type>;

    //
    // If std::pair<const K, V> and std::pair<K, V> are layout-compatible,
    // we can accept one or the other via slot_type. We are also free to
    // access the key via slot_type::key in this case.
    //
    static constexpr bool kIsLayoutCompatible = jstd::is_layout_compatible_kv<K, V>::value;

    using actual_value_type = typename std::conditional<kIsLayoutCompatible,
                                       init_type, value_type>::type;

    value_type          value;
    init_type           mutable_value;
    const key_type      key;
    key_type            mutable_key;

    map_slot_type() {}
    ~map_slot_type() {};
};

int main()
{
    map_slot_type<std::size_t, std::size_t> slot1;
    printf("sizeof(map_slot_type<std::size_t, std::size_t> = %d\n",
           (int)sizeof(slot1));

    map_slot_type<std::string, std::size_t> slot2;
    printf("sizeof(map_slot_type<std::string, std::size_t> = %d\n",
           (int)sizeof(slot2));

    return 0;
}
