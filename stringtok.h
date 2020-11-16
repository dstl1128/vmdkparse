//
// String Tokenizer
// Split std::basic_string with delimiter for each iteration.
// Tokenized strings are new strings and does not refer to original.
// Also does not modify original string during tokenizing.
//
// Author: Derek Saw
//
// Copyright (c) 2009. All rights reserved.
//

#ifndef __STRINGTOK_H
#define __STRINGTOK_H

//
// Conversations: Al-Go-Rithms
// from Jim Hyslop and Herb Sutter
// http://www.ddj.com/cpp/184403801
//


// TODO: delim.c_str() is not universal on containers
//       - only for std::basic_string<T> types
template <typename T>
class StringTok
{
public:
    StringTok(T const & seq, typename T::size_type pos = 0)
        : _seq(seq), _pos(pos) { }

    T operator () (T const & delim)
    {
        T token;
        if (_pos != T::npos)
        {
            // start of found token
            typename T::size_type first = _seq.find_first_not_of(delim.c_str(), _pos);
            if (first != T::npos)
            {
                // length of found token
                typename T::size_type len = _seq.find_first_of(delim.c_str(), first) - first;

                token = _seq.substr(first, len);

                // done; commit with non throw operation
                _pos = first + len;
                if (_pos != T::npos) ++_pos;
                if (_pos >= _seq.size()) _pos = T::npos;
            }
        }
        return token;
    }

private:
    const T & _seq;
    typename T::size_type _pos;
};

#endif
