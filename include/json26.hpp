#pragma once

#include <span>
#include <string>
#include <string_view>

#include <meta>

// We include parsing of some basic container types.
#include <variant>
#include <memory>
#include <vector>
#include <map>

#include <simd>

namespace json26
{
    struct parse_options {
        bool allow_trailing_comma = false; // make compile time?

        // force recombining of utf16 surrogate pairs into utf8
        // could otherwise potentially be a security issue if a surrogate pair slips through as something else.
        bool surrogate_reforming = true;
    };

    // Special marker type to just ignore everything parsed into it.
    struct ignorable {};

    // jsonnull is a special type to signify json-null's
    struct jsonnull {};

    // a type that can represent unspecified valid JSON values
    struct value : public std::variant<
            jsonnull,
            bool,
            double,
            std::string,
            std::vector<value>,
            std::map<std::string,value>
        >  {
    };

    // annotation marker to ignore members.
    struct ignore{};

    // annotation marker to rename entries in json.
    struct alias {
        char storage[100] = {};
        size_t const size;

        template<size_t N>
        constexpr alias(const char (&s)[N]) : size(N-1) {
            static_assert(N<sizeof(storage),"alias cannot be longer than storage");
            for(size_t i=0;i<N;i++)
                storage[i] = s[i];
        }
    };

    // marker annotation to indicate that member will consume extra properties.
    struct extra_props {};

    // main userfacing parsing entrypoint
    template <typename T>
    static inline bool parse(T &dest, const std::span<char> &in_data, parse_options options={} );


    namespace impl
    {
        struct parse_ctx
        {
            // state
            std::string tbuf; // used by consumers as temporary storage space.
            std::span<char> data; // span used instead of string_view for simd optimizations.

            bool typeerr = false;

            parse_options options;
        };

        template <typename T>
        bool parse(parse_ctx &, T &);

        inline bool skipspace(parse_ctx &ctx, size_t start_at = 0);
        static bool parse_ignore_any(parse_ctx& ctx);
        static bool parse_ignore_string(parse_ctx& ctx);
    }

    // main userfacing parsing entrypoint
    template <typename T>
    static inline bool parse(T &dest, const std::span<char> &in_data, parse_options options)
    {
        impl::parse_ctx ctx{
            .tbuf = "", 
            .data = in_data,
            .typeerr = false, 
            .options = options
        };

        // pre-skip spaces
        if (!skipspace(ctx))
            return false;

        // parse
        bool parse_ok = impl::parse<T>(ctx, dest);
        // but also verify that we've reached end-of-input without failure.
        bool spaced_to_end = !impl::skipspace(ctx);

        parse_ok = parse_ok && spaced_to_end;

        // TODO: add error-reporting callback or similar?
        // if (!parse_ok) {
        //     // where did we fail?
        //     std::string_view sv{ctx.data.data(), ctx.data.size() < 50 ? ctx.data.size() : 50};
        //     std::cerr<<"Failed at:"<<sv<<"\n";            
        // }

        return parse_ok;
    }

    namespace impl
    {
        using cvec = std::simd::basic_vec<char>;
        // inline void cur_line(
        //     const std::string_view message = "",
        //     const std::source_location& location = std::source_location::current())
        // {
        //     //std::cout << "[" << location.line() << "=>(" << message << ")]\n"; 
        // }


        // fast-check-value is a shortcut for quick-property-name checking/filtering by checking
        // the first 4 bytes statically with if's as a machine int32-compare.
        struct fcv_info
        {
            uint32_t acc, mask;
        };
        consteval fcv_info fast_check_values(std::string_view identifier) // std::meta::info member)
        {
            uint32_t acc = 0;
            uint32_t mask = 0;

            for (size_t i = 0; i <= identifier.size() && i < 4; i++)
            {
                uint32_t cval = i == identifier.size()
                                    ? '\"'
                                    : (unsigned char)identifier[i];
                acc |= (uint32_t)(cval << (i * 8));
                mask |= 0xff << (i * 8);
            }
            return {acc, mask};
        }

        static const cvec simd_space_vec = ' ';
        static const cvec simd_tab_vec = '\t';
        static const cvec simd_cr_vec = '\r';
        static const cvec simd_lf_vec = '\n';

        inline bool skipspace(parse_ctx &ctx, size_t start_at)
        {
            // Simd here seems to be a net-loss since skip-space is often used in places
            // where spaces do not occur(especially not with minified JSON)
            // ctx.data = ctx.data.subspan(start_at);
            // start_at = 0;
            // while(ctx.data.size()>=sizeof(cvec)) {
            //     cvec incoming = std::simd::partial_load<cvec>( ctx.data );

            //     // or in all matched spac-y characters
            //     auto spacemask =
            //         (incoming == simd_space_vec)
            //         |
            //         (incoming == simd_tab_vec)
            //         |
            //         (incoming == simd_cr_vec)
            //         |
            //         (incoming == simd_lf_vec);
                
            //     if (std::simd::all_of(spacemask)) {
            //         ctx.data = ctx.data.subspan(sizeof(cvec));
            //         continue;
            //     }
            //     // something wasn't zero
            //     int count = std::simd::reduce_min_index(!spacemask);

            //     // std::string_view sv{ctx.data.data(), ctx.data.size() < 50 ? ctx.data.size() : 50};
            //     // std::cout<<"Spc Count space:"<<count<<" at:"<<sv<<"\n";

            //     ctx.data = ctx.data.subspan(count);
            //     return true;
            // }
            size_t len = ctx.data.size();
            size_t idx = start_at;
            for (; idx < len; idx++)
            {
                switch (ctx.data[idx])
                {
                case '\r':
                case '\n':
                case '\t':
                case ' ':
                    continue;
                default:
                    ctx.data = ctx.data.subspan(idx);
                    return true;
                }
            }
            // we need to move forward even if we consumed everything!
            ctx.data = ctx.data.subspan(len);
            return false;
        }

        template<std::meta::info Member>
        consteval bool member_is_ignext(bool is_ign=true,bool is_ext=true) {
            return 
                (is_ign && 0 != std::meta::annotations_of_with_type(Member,^^ignore).size())
                ||
                // extra_props is auto-ignored since it should catch overflow (including "itself")
                (is_ext && 0 != std::meta::annotations_of_with_type(Member,^^extra_props).size())
                ;
        }

        template<typename T>
        consteval bool has_member_with_extra_props() {
            static constexpr auto members = std::define_static_array(
                        std::meta::nonstatic_data_members_of(
                            ^^T,
                            std::meta::access_context::current()));
            template for (constexpr auto member:members) 
            {
                if (0 != std::meta::annotations_of_with_type(member,^^extra_props).size())
                    return true;
            }
            return false;
        }
        
        template<std::meta::info Member>
        consteval std::string_view member_name_or_alias() {
            // search the member for an alias annotation
            template for (
                constexpr auto ann:
                std::define_static_array(std::meta::annotations_of_with_type(Member,^^alias))
                ) 
            {
                constexpr auto alias = std::meta::extract<json26::alias>(ann);
                return std::string_view(std::define_static_string(alias.storage));
            }
            // if not, use the members plain name.
            return std::meta::identifier_of(Member);
        }

        template<typename T>
        class is_variant : public std::false_type {};

        template<typename ...T>
        class is_variant<std::variant<T...>> : public std::true_type {
            using type = std::variant<T...>;
        };
        
        template<typename Ty>
        static bool parse_variant_part(parse_ctx&,Ty&,std::span<char>&) {
            return false;
        }

        template<typename Ty,typename H,typename ...R>
        static bool parse_variant_part(parse_ctx& ctx,Ty& tv,std::span<char>& checkpoint) {
            H v;
            if (parse(ctx,v)) {
                if constexpr (std::movable<H>) {
                    tv = std::move(v);
                } else {
                    tv = v;
                }
                return true;
            } else {
                // restore parsing point to checkpoint!
                // TODO: stop parse-cost-attacks with hard and soft error handling?
                ctx.data = checkpoint;
                return parse_variant_part<Ty,R...>(ctx,tv,checkpoint);
            }
        }



        template<typename ...T>
        inline bool parse_variant(parse_ctx& ctx,std::variant<T...>& v) {
            std::span<char> checkpoint = ctx.data;
            return parse_variant_part<std::variant<T...>,T...>(ctx,v,checkpoint);
        }

        template<typename T>
        inline bool parse(parse_ctx& ctx,typename is_variant<T>::type& v) {
            return parse_variant(ctx,v);
        }

        inline bool parse(parse_ctx& ctx,value& v) {
            return parse_variant(ctx,v);
        }

        template<typename ST,typename T>
        constexpr size_t unsafe_index_of(ST l,T item) {
            for (size_t i=0;i<l.size();i++)
                if (l[i]==item)
                    return i;
            return 0;
        }

        // The main reflection-enabled object parsing parser.
        // other template specializations handles things like numbers, maps, arrays, vectors, strings,etc
        template <typename T>
        inline bool parse(parse_ctx &ctx, T &dest)
        {
            //static_assert(!is_variant<std::variant<int,bool>>::value,"Uh-oh 1");
            // expecting object.
            if (ctx.data[0] != '{')
            {
                return false;
            }
            // skip { and subsequent spaces.
            if (!skipspace(ctx,1))
                return false;

            static constexpr auto members = std::define_static_array(
                        std::meta::nonstatic_data_members_of(
                            ^^T,
                            std::meta::access_context::current()));
            // Is it a security risk of duplicated property smuggling attacks, need a bit-set for found members?
            // Probably yes
            // - if we're upstream
            //   - objects should be re-formed, then ok (duplicates gone)
            //   - objects are checked but not re-formed, _can_ be an issue
            // - if we're downstream
            //   - if the upstream parsed/reformed well-formed then it should be gone
            //   - if the upstream checked but passed it should be rejected
            //   - if the upstream checked but not duplicates we _can_ have an issue.
            bool found[members.size()] = {};

            // now inside an object!
            while(true) {
                // any spaces must be skipped already at this point! (make sure at end-of-loop)

                // the next character must be an end-of-object or property name
                if (ctx.data[0]=='}')
                    break; // end-of-object, we're done.
                if (ctx.data[0]!='\"')
                    return false; // not a string!
                
                // First do fast-property-checking by "matching" a prefix.
                uint32_t prefix;
                if (ctx.data.size() >= 5)
                {
                    // fast path, mimicing an unaligned 32bit little-endian load as a compiler hint.
                    prefix =
                        (((uint32_t)(unsigned char)ctx.data[4]) << 24) 
                        | (((uint32_t)(unsigned char)ctx.data[3]) << 16) 
                        | (((uint32_t)(unsigned char)ctx.data[2]) << 8) 
                        | (((uint32_t)(unsigned char)ctx.data[1]));
                }
                else
                {
                    // with less than 5 letters left there can impossibly be an valid property value
                    // left in the input! >>>"":1}<<< is the shortest valid sequence and it's 5 letters
                    return false;
                }

                bool had_escape = false;
                // With the prefix loaded, do the fast-patch property match checks.
                template for (constexpr auto member : members)
                {
                    // do annotation checks
                    if constexpr (member_is_ignext<member>())
                        continue;
                    constexpr std::string_view identifier = member_name_or_alias<member>();
                    constexpr std::size_t id_size = identifier.size();
                    constexpr std::size_t mindex = unsafe_index_of(members,member);

                    // get the dword bits and mask to do a fast initial(or full) check
                    constexpr auto [acc, mask] = fast_check_values(identifier);
                    if ((prefix&mask)==acc) {
                        // prefix-match successful
                        if constexpr (id_size>3) {
                            // if longer than 3 letter,  first 4 letters 
                            // of property-name checks out, do a final "rest" check also
                            if (id_size+2 < ctx.data.size()) {
                                bool ok = true;
                                for(size_t idx=4;idx<id_size && ok;idx++) {
                                    //std::cout << ctx.data[idx+1] << "\n";
                                    char c = ctx.data[idx+1];
                                    ok = (c==identifier[idx]);
                                    had_escape = (!ok && (c=='\\' || (c&0x80)));
                                }
                                if (!ok)
                                    continue;
                                if (ctx.data[identifier.size()+1]!='\"')
                                    continue;
                            }
                        }
                        if (!skipspace(ctx,2+id_size))
                            return false;
                        if (ctx.data[0]!=':')
                            return false;
                        if (!skipspace(ctx,1)) // skip the :
                            return false;
                        if (found[mindex])
                            return false;
                        found[mindex] = true;
                        if (!parse(ctx,dest.[:member:]))
                            return false;
                        goto next_prop_check;
                    }
                }
                // had_escape checks if we're encountering a propery not found due to utf8 or otherwise escaped encoding.
                had_escape |= 
                    ((prefix>>24)&0xff)=='\\'
                    || ((prefix>>16)&0xff)=='\\'
                    || ((prefix>>8)&0xff)=='\\'
                    || ((prefix)&0xff)=='\\'
                    || (prefix&0x80808080) // UTF-8
                    ;
                if (had_escape || has_member_with_extra_props<T>()) {
                    // This path is entered both with uncertain parsing of escapes
                    // OR the existence of an extra_props catch-all
                    ctx.tbuf.resize(0);
                    if (!parse(ctx,ctx.tbuf))
                        return false;
                    if (ctx.data[0]!=':')
                        return false;
                    if (!skipspace(ctx,1)) // skip the :
                        return false;

                    if (had_escape) {
                        // re-check main properties with a slower string-compare
                        template for (constexpr auto member : members)
                        {
                            // do annotation checks
                            if constexpr (member_is_ignext<member>())
                                continue;

                            constexpr std::size_t mindex = unsafe_index_of(members,member);
                            constexpr std::string_view identifier = member_name_or_alias<member>();

                            if (identifier == ctx.tbuf) {
                                if (found[mindex])
                                    return false;
                                found[mindex] = true;
                                if (!parse(ctx,dest.[:member:]))
                                    return false;
                                goto next_prop_check;
                            }
                        }
                    }
                    // if we're still missing the property, it's time to store it in the extras 
                    template for (constexpr auto member : members)
                    {
                        if constexpr (member_is_ignext<member>(false,true)) {
                            std::string key = ctx.tbuf;
                            using VT = std::remove_reference_t<decltype(dest.[:member:]["prop"])>;
                            VT value;
                            if (!parse(ctx,value))
                                return false;
                            if constexpr (std::movable<VT>) {
                                auto [it,ok] = dest.[:member:].try_emplace(key,std::move(value));
                                if (!ok)
                                    return false;
                            } else {
                                auto [it,ok] = dest.[:member:].try_emplace(key,value);
                                if (!ok)
                                    return false;
                            }
                            goto next_prop_check;
                        }
                    }
                    // but if no extras are used, just ignore the value
                    if (!parse_ignore_any(ctx))
                        return false;
                    goto next_prop_check;
                }
                // If we've gotten this far then it's either a wildcard-catchable or ignorable property
                if (!parse_ignore_string(ctx))
                    return false;
                // check for :
                if (ctx.data.size()==0 || ctx.data[0]!=':')
                    return false;
                if (!skipspace(ctx,1)) // skip the :
                    return false;
                // now ignore the value
                if (!parse_ignore_any(ctx))
                    return false;
                // "fall-through"
                next_prop_check:
                if (ctx.data.size()==0)
                    return false;
                if (ctx.data[0]==',') {
                    skipspace(ctx,1);
                    if (ctx.data[0]=='}') {
                        if (ctx.options.allow_trailing_comma)
                            break; // accept the trailing comma as extraneous and just terminate the object
                        else
                            return false; // fail parsing for trailing commas.
                    }
                } else if (ctx.data[0]=='}') {
                    break;
                } else
                    return false;
            }


            // std::cout << "Pre Left:[" << ctx.data << "]\n";
            // skip terminating character
            skipspace(ctx,1);
            // now out of object, finished successfully!
            // std::cout << "Post Left:[" << ctx.data << "]\n";

            return true;
        }

        bool read_unicodehex(int& target, parse_ctx& ctx,int idx) {
            int c = 0;
            for (int i = 0 ; i<4 ; i++) {
                int tmp = ctx.data[idx+i];
                c = c << 4;
                if ('0' <= tmp && tmp <= '9')
                    c |= tmp - '0';
                else if ('A' <= tmp && tmp <= 'F')
                    c |= tmp - 'A' + 10;
                else if ('a' <= tmp && tmp <= 'f')
                    c |= tmp - 'a' + 10;
                else {
                    return false;
                }
            }
            target = c;
            return true;
        }

		// create a UTF8 sequence from a unicode codepoint
		// X is the type of our output
		template<typename X> inline void dump_utf8(X &x, uint32_t c) {
			if (c < 0x80) {
				// plain ascii are below 128
				x.push_back((char)c);
			} else {
				// recursive call to count and produce a heading byte indicating the needed number of bytes
				struct dump {
					void dmp(X &x, int used, unsigned int rest) {
						// test if the rest of the bits fits inside the UTF8 count mask
						if (rest&(used | ~0xff)) { // rest is masked by used bits and all above 8 bits, any bits remaining will trigger this branch
							// if not, we need more bits, try again with the 6 bottom bits chopped of and a new mask.
							dmp(x, used | (used >> 1), rest >> 6);
							// dump out our bits afterwardss
							x.push_back((char)(0x80 | (rest & 0x3f)));
						} else {
							// dump out the count mask and initial bits
							x.push_back((char)(((used << 1) | rest) & 0xff));
						}
					}
				} d;
				d.dmp(x, 0xc0, c);
			}
		}

        static const cvec simd_qoute_vec = '\"';
        static const cvec simd_slash_vec = '\\';


        template<typename T>
        static bool parse_string(parse_ctx& ctx,T& dest) {
            if (ctx.data[0]!='\"')
                return false; // not a string
            if (dest.size()>0)
                dest.resize(0);
            // try to do batches whenever we don't encounter "special" cases
            size_t srcstart = 1;
            size_t idx = 1;
            size_t len = ctx.data.size();

            cvec simd_idx_vec([](int i){ return (char)i; });

            while(true) {
                if (idx==len)
                    return false;
                // quick simd-check for directly ingestable text.
                if ((len-idx)>sizeof(cvec)) {
                    cvec incoming = std::simd::partial_load<cvec>( ctx.data.subspan(idx) );

                    auto quote_match = incoming == simd_qoute_vec;
                    auto slash_match = incoming == simd_slash_vec;

                    auto combined_match = quote_match | slash_match;

                    if (std::simd::none_of(combined_match)) {
                        // none of the next vec-size characters are slash or quote characters so we can just consume them.
                        idx+=sizeof(cvec);
                        continue;
                    }

                    int count = std::simd::reduce_min_index(combined_match);

                    // std::string_view sv{ctx.data.subspan(idx).data(), ctx.data.subspan(idx).size() < 50 ? ctx.data.subspan(idx).size() : 50};
                    // std::cout<<"Count:"<<count<<" at "<<sv<<"\n";

                    idx+=count;
                    goto eat_slow;
                }
                eat_slow:
                char c = ctx.data[idx];
                if (c!='\\' && c!='\"') {
                    idx++;
                    continue;
                }
                if (idx!=srcstart) {
                    dest.append(ctx.data.data()+srcstart,idx-srcstart);
                    srcstart = idx;
                }
                if (c=='\"')
                    break;
                if (c!='\\' || (idx+2)>=len)
                    return false; // TODO: maybe put some invalid state assert here?
                switch(ctx.data[idx+1]) {
                    case '\"' :
                    case '\\' :
                    case '/' :
                        dest.push_back(ctx.data[idx+1]);
                        srcstart = idx = idx+2;
                        continue;
                    case 'b' :
                        srcstart = idx = idx+2;
                        dest.push_back('\b');
                        continue;
                    case 'f' :
                        srcstart = idx = idx+2;
                        dest.push_back('\f');
                        continue;
                    case 'n' :
                        srcstart = idx = idx+2;
                        dest.push_back('\n');
                        continue;
                    case 'r' :
                        srcstart = idx = idx+2;
                        dest.push_back('\r');
                        continue;
                    case 't' :
                        srcstart = idx = idx+2;
                        dest.push_back('\t');
                        continue;
                    case 'u' :
                    {
                        if ((idx+6)>=len)
                            return false;
                        int p1 = 0;
                        if (!read_unicodehex(p1,ctx,idx+2))
                            return false;
                        srcstart = idx = idx+6;

                        if (ctx.options.surrogate_reforming && p1 >= 0xd800 && p1 < 0xdc00) {
                            // Utf16 surrogate pair, required to be decoded into a single utf8 codepoint
                            if ((idx+6)>=len || ctx.data[idx]!='\\' || ctx.data[idx+1]!='u')
                                return false;
                            int p2 = 0;
                            if (!read_unicodehex(p2,ctx,idx+2))
                                return false;
                            if (!(p2 >= 0xdc00 && p2 < 0xe000)) {
								// invalid secondary surrogate pair character
								return false;
							}
                            srcstart = idx = idx+6;
							p1 = (((p1 & 0x3ff) << 10) | (p2 & 0x3ff)) + 0x10000;
                        }
                        dump_utf8(dest,p1);
                        continue;
                    }
                    default:
                        return false; // invalid escape sequence!
                }
            }
            skipspace(ctx,idx+1); // skip string and trailing spaces
            return true;
        }

        template<>
        bool parse<std::string>(parse_ctx& ctx,std::string& dest) {
            return parse_string(ctx,dest);
        }

        template<typename T,bool is_float,bool is_signed>
        bool parse_number(parse_ctx& ctx,T& dest) {
            T sign = 1;
            T acc = 0;
            //std::cout << "Number parsing?\n";
            size_t len = ctx.data.size();
            bool digits = false;
            size_t idx=0;
            for (;idx<len;idx++) {
                char c = ctx.data[idx];
                //std::cout << "Letter:" << c <<"\n";
                if (idx==0) {
                    // if (c=='+') {
                    //     continue;
                    // } else 
                    if constexpr (is_signed) {
                        if (c=='-') {
                            sign = -1;
                            continue;
                        }
                    } else {
                        if (c=='-') {
                            return false;
                        }
                    }
                }
                if (c<'0' || '9'<c) {
                    break;
                }
                digits = true;
                acc = (acc*10) + sign*(c-'0');
            }
            if (!digits)
                return false;
            // TODO: do we check for dot or exponent here? (the parser will fail later.. although allowing them could be good?)
            if constexpr (is_float) {
                if (idx<len && ctx.data[idx]=='.') {
                    idx++;
                    for(;idx<len;idx++) {
                        char c = ctx.data[idx];
                        if (c<'0' || '9'<c) {
                            break;
                        }
                        sign /= 10;
                        acc += sign*(c-'0');
                    }
                }
            }
            dest = acc;
            skipspace(ctx,idx);
            //std::cout << "Parsed number:" << dest << "\n";
            return true;
        }

        template<>
        inline bool parse<int32_t>(parse_ctx& ctx,int32_t& dest) {
            return parse_number<int32_t,false,true>(ctx,dest);
        }
        template<>
        inline bool parse<int64_t>(parse_ctx& ctx,int64_t& dest) {
            return parse_number<int64_t,false,true>(ctx,dest);
        }

        template<>
        inline bool parse<uint32_t>(parse_ctx& ctx,uint32_t& dest) {
            return parse_number<uint32_t,false,true>(ctx,dest);
        }
        template<>
        inline bool parse<uint64_t>(parse_ctx& ctx,uint64_t& dest) {
            return parse_number<uint64_t,false,true>(ctx,dest);
        }

        template<>
        inline bool parse<float>(parse_ctx& ctx,float& dest) {
            return parse_number<float,true,true>(ctx,dest);
        }
        template<>
        inline bool parse<double>(parse_ctx& ctx,double& dest) {
            return parse_number<double,true,true>(ctx,dest);
        }


        struct ignorable_string {
            inline int size() { return 0; }
            inline void resize(size_t) {}
            inline void push_back(char) {}
            inline void append(const char*,size_t) {}
        };


        static bool parse_ignore_string(parse_ctx& ctx) {

            ignorable_string igns;
            return parse_string(ctx,igns);
        }

        template<>
        inline bool parse<ignorable_string>(parse_ctx& ctx,ignorable_string& igns) {
            return parse_string(ctx,igns);
        }


        // parses objects into something that behaves like std::map<std::string,..>
        template <typename M,typename K,typename V>
        inline bool parse_map(parse_ctx &ctx, M &dest)
        {
            // cur_line();
            // expecting object.
            if (ctx.data[0] != '{')
            {
                return false;
            }
            // cur_line();
            // skip { and subsequent spaces.
            if (!skipspace(ctx,1))
                return false;

            // cur_line();
            // now inside an object!
            while(true) {
                // any spaces must be skipped already at this point! (make sure at end-of-loop)

                // cur_line();
                // the next character must be an end-of-object or property name
                if (ctx.data[0]=='}')
                    break; // end-of-object, we're done.
                if (ctx.data[0]!='\"')
                    return false; // not a string!
                
                // cur_line();
                // Parse our key (preferably! a string)
                K key;
                if (!parse(ctx,key))
                    return false;
                // check for :
                if (ctx.data.size() == 0 || ctx.data[0]!=':')
                    return false;
                if (!skipspace(ctx,1)) // skip the :
                    return false;
                // cur_line();

                // now ignore the value
                V value;
                if (!parse(ctx,value))
                    return false;
                // insert the parsed value
                if constexpr (std::movable<V>) {
                    auto [it,ok] = dest.try_emplace(key,std::move(value));
                    if (!ok)
                        return false; // disallow duplicate keys
                } else {
                    auto [it,ok] = dest.try_emplace(key,value);
                    if (!ok)
                        return false; // disallow duplicate keys
                }
                if (ctx.data.size()==0)
                    return false;
                if (ctx.data[0]==',') {
                    skipspace(ctx,1);
                    if (ctx.data[0]=='}') {
                        if (ctx.options.allow_trailing_comma)
                            break; // accept the trailing comma as extraneous and just terminate the object
                        else
                            return false; // fail parsing for trailing commas.
                    }
                } else if (ctx.data[0]=='}')
                    break;
                else
                    return false;
            }

            // skip terminating character
            skipspace(ctx,1);
            // now out of object, finished successfully!

            return true;
        }


        // parses array into something that behaves like std::vector
        template<typename AT,typename IT>
        static bool parse_array(parse_ctx& ctx,AT& dest) {
            if (ctx.data[0]!='[')
                return false;
            if (!skipspace(ctx,1))
                return false;
            while(true) {
                if (ctx.data[0]==']')
                    break;
                //std::cout<<">> Doing array\n";
                IT item;
                if (!parse(ctx,item))
                    return false;
                //std::cout<<">> Done Item\n";
                if constexpr (std::movable<IT>) {
                    dest.push_back(std::move(item));
                } else {
                    dest.push_back(item);
                }
                if (ctx.data.size()==0)
                    return false;
                if (ctx.data[0]==',') {
                    if (!skipspace(ctx,1))
                        return false;
                    if (ctx.data[0]==']') {
                        if (ctx.options.allow_trailing_comma)
                            break;
                        else
                            return false;
                    }
                } else if (ctx.data[0]==']')
                    break;
                else
                    return false;
            }
            skipspace(ctx,1); // eat ]
            return true;
        }

        struct ignorable_map {
            template<typename K,typename V>
            std::pair<int,bool> try_emplace(K&&,V&&) { return std::make_pair(0,true); }
        };
        struct ignorable_array {
            template<typename T>
            void push_back(const T&){}
            template<typename T>
            void push_back(T&&) {}
        };
        static bool ignore_array(parse_ctx& ctx) {
            ignorable_array arr;
            return parse_array<ignorable_array,ignorable>(ctx,arr);
        }
        static bool ignore_object(parse_ctx& ctx) {
            ignorable_map map;
            return parse_map<ignorable_map,ignorable_string,ignorable>(ctx,map);
        }

        static bool ignore_match(parse_ctx& ctx,const std::string_view& str){
            if (ctx.data.size()<str.size())
                return false;
            std::string_view cv{ctx.data.data(),str.size()};
            // ctx.data.subview(0,str.size())
            //std::cout<<cv<<"<- \n";
            if (cv != str)
                return false;
            skipspace(ctx,str.size());
            return true;
        }

        template<typename V>
        static bool parse(parse_ctx& ctx, std::vector<V>& v) {
            return parse_array<std::vector<V>,V>(ctx,v);
        }
        template<typename K,typename V>
        static bool parse(parse_ctx& ctx, std::map<K,V>& v) {
            return parse_map<std::map<K,V>,K,V>(ctx,v);
        }

        static bool parse(parse_ctx& ctx, jsonnull&) {
            if (ctx.data.size()>0 && ctx.data[0]=='n')
                return ignore_match(ctx,"null");
            return false;
        }
        static bool parse(parse_ctx& ctx, bool& v) {
            if (ctx.data.size()>0) {
                if (ctx.data[0]=='t') {
                    v = true;
                    return ignore_match(ctx,"true");
                } else if (ctx.data[0]=='f') {
                    v = false;
                    return ignore_match(ctx,"false");
                }
            }
            return false;
        }


        static bool parse_ignore_any(parse_ctx& ctx) {
            switch(ctx.data[0]) {
            case '{' :
                return ignore_object(ctx);
            case '[' :
                return ignore_array(ctx);
            case '\"' : 
                return parse_ignore_string(ctx);
            case 't' :
                return ignore_match(ctx,"true");
            case 'f' :
                return ignore_match(ctx,"false");
            case 'n' :
                return ignore_match(ctx,"null");
            case '0' : case '1' : case '2' : case '3' : case '4' :
            case '5' : case '6' : case '7' : case '8' : case '9' :
            case '-' : {
                double dummy;
                return parse(ctx,dummy);
                // TODO: faster? return ignore_number(ctx);
            }
            default:
                return false;
            }
        }

        template<>
        inline bool parse<ignorable>(parse_ctx& ctx,ignorable&) {
            return parse_ignore_any(ctx);
        }

    }

}
