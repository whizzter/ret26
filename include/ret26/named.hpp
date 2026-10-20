#include "static_string.hpp"

// helper to create named objects like std::make_tuple but with names.
// e.g. auto x = ret26::make_named<"hello","world">(12, 34.56);
namespace ret26 {

    namespace namedimpl {
        template<ret26::static_string ...Names>
        struct named_type_maker {

            template<typename ...T>
            struct with_types {
                struct type;

                consteval {
                    std::vector<ret26::static_string> names = { Names... };
                    std::vector<std::meta::info> types = { (^^T)... };

                    std::vector<std::meta::info> dest;

                    static_assert(sizeof...(Names)==sizeof...(T),"names and types not matching in length");

                    for(size_t idx=0;idx<names.size();idx++ ){
                        dest.push_back(
                            std::meta::data_member_spec(
                                types[idx],
                                {.name = names[idx].as_view() })
                        );
                    }

                    std::meta::define_aggregate(^^type,dest);            
                }
            };
        };


    }

    // make a object obj with property names of types supplied in the call.
    template<ret26::static_string ...Names,typename ...T>
    auto make_named(T&&... args) {
        using ntm = namedimpl::named_type_maker<Names...>;
        using named = typename ntm::with_types<T...>::type;
        return named{args...};
    }

}