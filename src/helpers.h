#ifndef HELPERS_H
#define HELPERS_H

#include <nonstd/string_view.hpp>
#include <jinja2cpp/string_helpers.h>

#include <string>
#include <type_traits>
#include <cwchar>

namespace jinja2
{
struct MultiStringLiteral
{
    const char* charValue;
    const wchar_t* wcharValue;

    template<typename CharT>
    auto GetValue() const
    {
        auto memPtr = SelectMemberPtr<CharT, &MultiStringLiteral::charValue, &MultiStringLiteral::wcharValue>::GetPtr();
        return std::basic_string<CharT>(this->*memPtr);
    }

    template<typename CharT, const char* MultiStringLiteral::*, const wchar_t* MultiStringLiteral::*>
    struct SelectMemberPtr;

    template<const char* (MultiStringLiteral::*charMemPtr), const wchar_t* (MultiStringLiteral::*wcharMemPtr)>
    struct SelectMemberPtr<char, charMemPtr, wcharMemPtr>
    {
        static auto GetPtr() {return charMemPtr;}
    };

    template<const char* (MultiStringLiteral::*charMemPtr), const wchar_t* (MultiStringLiteral::*wcharMemPtr)>
    struct SelectMemberPtr<wchar_t, charMemPtr, wcharMemPtr>
    {
        static auto GetPtr() {return wcharMemPtr;}
    };

    template<typename CharT>
    friend std::basic_ostream<CharT>& operator << (std::basic_ostream<CharT>& os, const MultiStringLiteral& lit)
    {
        os << lit.GetValue<CharT>();
        return os;
    }
};

#define UNIVERSAL_STR(Str) MultiStringLiteral{Str, L##Str}

//! CompileEscapes replaces escape characters by their meanings.
/**
 * @param[in] s Characters sequence with zero or more escape characters.
 * @return Characters sequence copy where replaced all escape characters by
 * their meanings.
 */
template<typename Sequence>
Sequence CompileEscapes(Sequence s)
{
   auto itr1 = s.begin();
   auto itr2 = s.begin();
   const auto end = s.cend();

   auto removalCount  = 0;

   while (end != itr1)
   {
      if ('\\' == *itr1)
      {
         ++removalCount;

         if (end == ++itr1)
            break;
         if ('\\' != *itr1)
         {
            switch (*itr1)
            {
               case 'n': *itr1 = '\n'; break;
               case 'r': *itr1 = '\r'; break;
               case 't': *itr1 = '\t'; break;
               default:                break;
            }

            continue;
         }
      }

      if (itr1 != itr2)
         *itr2 = *itr1;

      ++itr1;
      ++itr2;
   }

   s.resize(s.size() - removalCount);

   return s;
}

} // jinja2

#endif // HELPERS_H
