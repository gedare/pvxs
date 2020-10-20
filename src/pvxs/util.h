/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_UTIL_H
#define PVXS_UTIL_H

#include <map>
#include <deque>
#include <functional>
#include <ostream>
#include <type_traits>

#include <osiSock.h>
#include <epicsEvent.h>
#include <epicsMutex.h>
#include <epicsGuard.h>

#include <event2/util.h>

#ifdef _WIN32
#  include <ws2ipdef.h>
#endif

#include <pvxs/version.h>

namespace pvxs {

namespace detail {
// ref. wrapper to mark string for escaping
class Escaper
{
    const char* val;
    size_t count;
    friend
    PVXS_API
    std::ostream& operator<<(std::ostream& strm, const Escaper& esc);
public:
    PVXS_API explicit Escaper(const char* v);
    constexpr explicit Escaper(const char* v, size_t l) :val(v),count(l) {}
};

PVXS_API
std::ostream& operator<<(std::ostream& strm, const Escaper& esc);

} // namespace detail

//! Print string to output stream with non-printable characters escaped.
//!
//! Outputs (almost) C-style escapes.
//! Prefers short escapes for newline, tab, quote, etc ("\\n").
//! Falls back to hex escape (eg. "\xab").
//!
//! Unlike C, hex escapes are always 2 chars.  eg. the output "\xabcase"
//! would need to be manually changed to "\xab""case" to be used as C source.
//!
//! @code
//!   std::string blah("this \"is a test\"");
//!   std::cout<<pvxs::escape(blah);
//! @endcode
inline detail::Escaper escape(const std::string& s) {
    return detail::Escaper(s.c_str(), s.size());
}
//! Print nil terminated char array to output stream with non-printable characters escaped.
//! @code
//!   std::cout<<pvxs::escape("this \"is a test\"");
//! @endcode
inline detail::Escaper escape(const char* s) {
    return detail::Escaper(s);
}
//! Print fixed length char array to output stream with non-printable characters escaped.
//! @code
//!   std::cout<<pvxs::escape("this \"is a test\"", 6);
//!   // prints 'this \"'
//! @endcode
inline detail::Escaper escape(const char* s,size_t n) {
    return detail::Escaper(s,n);
}

#if !defined(__rtems__) && !defined(vxWorks)

/** minimal portable process signal handling in CLI tools.
 *
 * @code
 *     epicsEvent evt;
 *     SigInt handle([&evt]() {
 *          evt.trigger();
 *     });
 *     ... setup network operations
 *     evt.wait();
 *     // completion, or SIGINT
 * @endcode
 */
class PVXS_API SigInt {
    void (*prevINT)(int);
    void (*prevTERM)(int);
    std::function<void()> handler;
    static void _handle(int);
public:
    SigInt(decltype (handler)&& handler);
    ~SigInt();
};

#else // !defined(__rtems__) && !defined(vxWorks)

class SigInt {
public:
    SigInt(std::function<void()>&& handler) {}
}

#endif // !defined(__rtems__) && !defined(vxWorks)

//! return a snapshot of internal instance counters
PVXS_API
std::map<std::string, size_t> instanceSnapshot();

//! See Indented
struct indent {};

PVXS_API
std::ostream& operator<<(std::ostream& strm, const indent&);

//! Scoped indentation for std::ostream
struct PVXS_API Indented {
    explicit Indented(std::ostream& strm, int depth=1);
    Indented(const Indented&) = delete;
    Indented(Indented&& o) noexcept
        :strm(o.strm)
        ,depth(o.depth)
    {
        o.strm = nullptr;
        o.depth = 0;
    }
    ~Indented();
private:
    std::ostream *strm;
    int depth;
};

struct PVXS_API Detailed {
    explicit Detailed(std::ostream& strm, int lvl=1);
    Detailed(const Detailed&) = delete;
    Detailed(Detailed&& o) noexcept
        :strm(o.strm)
        ,lvl(o.lvl)
    {
        o.strm = nullptr;
        o.lvl = 0;
    }
    ~Detailed();
    static
    int level(std::ostream& strm);
private:
    std::ostream *strm;
    int lvl;
};

/** Thread safe multi-producer, single-consumer queue
 *
 * If constructed with limit>0, then bounded.
 */
template<typename T>
class MPSCFIFO {
    epicsMutex lock;
    epicsEvent notifyW, notifyR;
    std::deque<T> Q;
    size_t nlimit=0u;
    unsigned nwriters=0u;

    typedef epicsGuard<epicsMutex> Guard;
    typedef epicsGuardRelease<epicsMutex> UnGuard;
public:
    typedef T value_type;

    MPSCFIFO() = default;
    explicit MPSCFIFO(size_t limit) :nlimit(limit) {}

    /** Move a new element into the queue.
     *
     * A bounded queue will block push() while full.
     */
    void push(T&& ent) {
        bool wakeup;
        {
            Guard G(lock);
            while(nlimit && Q.size()>=nlimit) {
                nwriters++;
                {
                    UnGuard U(G);
                    notifyW.wait();
                }
                nwriters--;
            }
            wakeup = Q.empty();
            Q.push_back(std::move(ent));
        }
        if(wakeup)
            notifyR.signal();
    }

    /** Remove an element from the queue.
     *
     * Blocks while queue is empty.
     */
    T pop() {
        bool wakeup;
        T ret;
        {
            Guard G(lock);
            while(Q.empty()) {
                UnGuard U(G);
                notifyR.wait();
            }
            wakeup = nwriters && nlimit && Q.size()<nlimit;
            ret = std::move(Q.front());
            Q.pop_front();
        }
        if(wakeup)
            notifyW.signal();
        return ret;
    }

    class iterator {
        MPSCFIFO *Q = nullptr;
        bool isBegin=true;
        iterator(MPSCFIFO* Q, bool isBegin) :Q(Q), isBegin(isBegin) {}
        friend class MPSCFIFO;
    public:
        iterator() = default;
        iterator& operator++() {
            // This is an infinite iterator
            return *this;
        }
        iterator operator++(int) {
            iterator ret(*this);
            ++(*this);
            return ret;
        }
        T operator*() const {
            return Q->pop(); // all this boilerplate, just to pop()...
        }
        bool operator==(const iterator& o) const { return isBegin==o.isBegin; }
        bool operator!=(const iterator& o) const { return !(o==*this); }
    };

    //! Support "iteration" of queue.  One way to express a loop calling pop()
    iterator begin() { return iterator(this, true); }
    iterator end() { return iterator(this, false); }
};

} // namespace pvxs

#endif // PVXS_UTIL_H
