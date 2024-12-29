/************************************************************************************

  CC BY-SA 4.0 License

  Copyright (c) 2024 XiongHui Guo (gz_shines at msn.com)

  https://github.com/shines77/jstd_cluster_flat_map
  https://gitee.com/shines77/jstd_cluster_flat_map

*************************************************************************************

  CC Attribution-ShareAlike 4.0 International

  https://creativecommons.org/licenses/by-sa/4.0/deed.en

  You are free to:

    1. Share -- copy and redistribute the material in any medium or format.

    2. Adapt -- remix, transforn, and build upon the material for any purpose,
    even commerically.

    The licensor cannot revoke these freedoms as long as you follow the license terms.

  Under the following terms:

    * Attribution -- You must give appropriate credit, provide a link to the license,
    and indicate if changes were made. You may do so in any reasonable manner,
    but not in any way that suggests the licensor endorses you or your use.

    * ShareAlike -- If you remix, transform, or build upon the material, you must
    distribute your contributions under the same license as the original.

    * No additional restrictions -- You may not apply legal terms or technological
    measures that legally restrict others from doing anything the license permits.

  Notices:

    * You do not have to comply with the license for elements of the material
    in the public domain or where your use is permitted by an applicable exception
    or limitation.

    * No warranties are given. The license may not give you all of the permissions
    necessary for your intended use. For example, other rights such as publicity,
    privacy, or moral rights may limit how you use the material.

************************************************************************************/

#ifndef JSTD_HASHMAP_CLUSTER_FLAT_TABLE_HPP
#define JSTD_HASHMAP_CLUSTER_FLAT_TABLE_HPP

#pragma once

namespace jstd {

template <typename Key, typename Value>
class flat_map_type_policy
{
public:
    typedef Key                                             key_type;
    typedef Value                                           mapped_type;
    typedef typename std::remove_const<Key>::type           raw_key_type;
    typedef typename std::remove_const<Value>::type         raw_mapped_type;

    typedef std::pair<raw_key_type, raw_mapped_type>        init_type;
    typedef std::pair<raw_key_type &&, raw_mapped_type &&>  moved_type;
    typedef std::pair<const Key, Value>                     value_type;

    typedef value_type                                      element_type;

    typedef flat_map_type_policy<Key, Value>                this_type;
};

} // namespace jstd

#endif // JSTD_HASHMAP_CLUSTER_FLAT_TABLE_HPP
