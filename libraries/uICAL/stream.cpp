/*############################################################################
# Copyright (c) 2020 Source Simian  :  https://github.com/sourcesimian/uICAL #
############################################################################*/
#include "uICAL/cppstl.h"
#include "uICAL/stream.h"
#include "uICAL/string.h"

#ifdef ARDUINO
    #include <Arduino.h>
#endif

namespace uICAL {
    ostream& ostream::operator <<(const ostream& stm) {
        for (string st : stm.strings) {
            this->strings.push_back(st);
        }
        return *this;
    }

    ostream& ostream::operator <<(const char* st) {
        this->strings.push_back(st);
        return *this;
    }

    ostream& ostream::operator <<(const string& st) {
        this->strings.push_back(st);
        return *this;
    }

    ostream& ostream::operator <<(char ch) {
        this->strings.push_back(string::fmt("%c", ch));
        return *this;
    }

    ostream& ostream::operator <<(int i) {
        this->strings.push_back(string::fmt("%d", i));
        return *this;
    }

    ostream& ostream::operator <<(unsigned int i) {
        this->strings.push_back(string::fmt("%u", i));
        return *this;
    }

    ostream& ostream::operator <<(long long int i) {
        this->strings.push_back(string::fmt("%lld", i));
        return *this;
    }

    ostream::operator string() const {
        return this->str();
    }

    bool ostream::empty() const {
        return this->strings.size() == 0;
    }

    void ostream::clear() {
        this->strings.clear();
    }

    string ostream::str() const {
        string ret = "";
        for (string st : this->strings) {
            ret += st;
        }
        return ret;
    }

    #ifdef ARDUINO

        istream_Stream::istream_Stream(Stream& stm)
        : stm(stm)
        {}

        // A network Stream delivers its body in bursts, so available() can
        // momentarily report zero in the middle of the data. Treating that as
        // end-of-input would silently split a line in two, so wait for the
        // next burst and only give up once the timeout expires.
        bool istream_Stream::waitAvailable() const {
            if (this->stm.available()) {
                return true;
            }
            unsigned long timeout = this->stm.getTimeout();
            unsigned long start = millis();
            while (millis() - start < timeout) {
                if (this->stm.available()) {
                    return true;
                }
                delay(1);
            }
            return false;
        }

        char istream_Stream::peek() const {
            if (!this->waitAvailable()) {
                return (char)-1;
            }
            return (char)this->stm.peek();
        }

        char istream_Stream::get() {
            if (!this->waitAvailable()) {
                return (char)-1;
            }
            return (char)this->stm.read();
        }

        bool istream_Stream::readuntil(string& st, char delim, size_t maxLen) {
            // Read one byte at a time rather than via readBytesUntil().
            // readBytesUntil() returns a short count both when it reached the
            // delimiter and when it simply timed out mid-line, and the caller
            // cannot tell the two apart. On a bursty network connection that
            // silently splits one line into two: "TZNAME:GMT+2" arriving in
            // two TCP segments becomes "TZNAME" followed by ":GMT+2", and the
            // fragment without a ':' then fails to parse as a VLINE.
            // Ending a line only on a real delimiter removes that ambiguity.
            st = "";
            bool got_any = false;

            while (true) {
                if (!this->waitAvailable()) {
                    break;
                }
                int ch = this->stm.read();
                if (ch < 0) {
                    break;
                }
                got_any = true;
                if ((char)ch == delim) {
                    return true;
                }
                if (maxLen == 0 || st.length() < maxLen) {
                    st += (char)ch;
                }
            }
            return got_any;
        }

        istream_String::istream_String(const String& st)
        : st(st)
        , pos(0)
        {}

        char istream_String::peek() const {
            return this->st.charAt(this->pos);
        }

        char istream_String::get() {
            return this->st.charAt(this->pos++);
        }

        bool istream_String::readuntil(string& st, char delim, size_t maxLen) {
            if (this->pos >= this->st.length()) {
                return false;
            }

            size_t index = this->st.indexOf(delim, this->pos);
            if (index == (size_t)-1) {
                st = this->st.substring(this->pos);
                this->pos = this->st.length();
            }
            else {
                st = this->st.substring(this->pos, index);
                this->pos = index + 1;
            }
            // Truncate if maxLen specified
            if (maxLen > 0 && st.length() > maxLen) {
                st = st.substr(0, maxLen);
            }
            return true;
        }

    #else

        istream_stl::istream_stl(std::istream& istm)
        : istm(istm)
        {}

        char istream_stl::peek() const {
            return this->istm.peek();
        }

        char istream_stl::get() {
            return this->istm.get();
        }

        bool istream_stl::readuntil(string& st, char delim, size_t maxLen) {
            if (std::getline(this->istm, st, delim)) {
                // Truncate if maxLen specified
                if (maxLen > 0 && st.length() > maxLen) {
                    st = st.substr(0, maxLen);
                }
                return true;
            }
            return false;
        }

    #endif
}
