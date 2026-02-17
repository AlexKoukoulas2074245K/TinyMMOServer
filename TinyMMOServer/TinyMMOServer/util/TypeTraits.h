///------------------------------------------------------------------------------------------------
///  TypeTraits.h
///  TinyMMOServer
///
///  Created by Alex Koukoulas on 20/09/2023.
///-----------------------------------------------------------------------------------------------

#ifndef TypeTraits_h
#define TypeTraits_h

///-----------------------------------------------------------------------------------------------

#include <string>
#include <typeindex>
#include <utility>

///-----------------------------------------------------------------------------------------------

static std::hash<std::string> hashFunction;

///-----------------------------------------------------------------------------------------------

class TypeID
{
    static size_t counter;

public:
    template<class T>
    static size_t value()
    {
        static size_t id = counter++;
        return id;
    }
};

///-----------------------------------------------------------------------------------------------
/// Compute a unique integer id for a given template class.
/// @tparam T the type class to generate the unique id for.
/// @returns a unique id for given template parameter class type.
template<class T>
inline std::size_t GetTypeHash()
{
    return TypeID::value<T>();
}



///-----------------------------------------------------------------------------------------------

#endif /* TypeTraits_h */

