/*
 * Copyright (C) 2017, Yeolar
 */

// @author Andrei Alexandrescu (andrei.alexandrescu@fb.com)

/**
 * Converts anything to anything, with an emphasis on performance and
 * safety.
 */

#pragma once

#include <algorithm>
#include <type_traits>
#include <limits>
#include <string>
#include <tuple>
#include <stdexcept>
#include <cmath>
#include <limits.h>
#include <boost/implicit_cast.hpp>
#include "rddoc/util/Macro.h"
#include "rddoc/util/Range.h"

// V8 JavaScript implementation
#include "rddoc/3rd/double-conversion/double-conversion.h"

#define RDD_RANGE_CHECK(condition, message, src)              \
  ((condition) ? (void)0 : throw std::range_error(            \
    (std::string(__FILE__ "(" RDD_STRINGIZE2(__LINE__) "): ") \
     + (message) + ": '" + (src) + "'").c_str()))

#define RDD_RANGE_CHECK_BEGIN_END(condition, message, b, e)   \
  RDD_RANGE_CHECK(condition, message, std::string((b), (e) - (b)))

#define RDD_RANGE_CHECK_STRINGPIECE(condition, message, sp)   \
  RDD_RANGE_CHECK(condition, message, std::string((sp).data(), (sp).size()))

namespace rdd {

/**
 * The identity conversion function.
 * to<T>(T) returns itself for all types T.
 */
template <class Tgt, class Src>
typename std::enable_if<std::is_same<Tgt, Src>::value, Tgt>::type
to(const Src & value) {
  return value;
}

template <class Tgt, class Src>
typename std::enable_if<std::is_same<Tgt, Src>::value, Tgt>::type
to(Src && value) {
  return std::move(value);
}

/*******************************************************************************
 * Integral to integral
 ******************************************************************************/

/**
 * Checked conversion from integral to integral. The checks are only
 * performed when meaningful, e.g. conversion from int to long goes
 * unchecked.
 */
template <class Tgt, class Src>
typename std::enable_if<
  std::is_integral<Src>::value
  && std::is_integral<Tgt>::value
  && !std::is_same<Tgt, Src>::value,
  Tgt>::type
to(const Src & value) {
  /* static */ if (std::numeric_limits<Tgt>::max()
                   < std::numeric_limits<Src>::max()) {
    RDD_RANGE_CHECK(value <= std::numeric_limits<Tgt>::max(),
      "Overflow",
      std::to_string(value));
  }
  /* static */ if (std::is_signed<Src>::value &&
                   (!std::is_signed<Tgt>::value || sizeof(Src) > sizeof(Tgt))) {
    RDD_RANGE_CHECK(value >= std::numeric_limits<Tgt>::min(),
      "Negative overflow",
      std::to_string(value));
  }
  return static_cast<Tgt>(value);
}

/*******************************************************************************
 * Floating point to floating point
 ******************************************************************************/

template <class Tgt, class Src>
typename std::enable_if<
  std::is_floating_point<Tgt>::value
  && std::is_floating_point<Src>::value
  && !std::is_same<Tgt, Src>::value,
  Tgt>::type
to(const Src & value) {
  /* static */ if (std::numeric_limits<Tgt>::max() <
                   std::numeric_limits<Src>::max()) {
    RDD_RANGE_CHECK(value <= std::numeric_limits<Tgt>::max(),
                    "Overflow",
                    std::to_string(value));
    RDD_RANGE_CHECK(value >= -std::numeric_limits<Tgt>::max(),
                    "Negative overflow",
                    std::to_string(value));
  }
  return boost::implicit_cast<Tgt>(value);
}

/*******************************************************************************
 * Anything to string
 ******************************************************************************/

namespace detail {

template <class T>
const T& getLastElement(const T & v) {
  return v;
}

template <class T, class... Ts>
typename std::tuple_element<
  sizeof...(Ts),
  std::tuple<T, Ts...> >::type const&
  getLastElement(const T&, const Ts&... vs) {
  return getLastElement(vs...);
}

// This class exists to specialize away std::tuple_element in the case where we
// have 0 template arguments. Without this, Clang/libc++ will blow a
// static_assert even if tuple_element is protected by an enable_if.
template <class... Ts>
struct last_element {
  typedef typename std::enable_if<
    sizeof...(Ts) >= 1,
    typename std::tuple_element<
      sizeof...(Ts) - 1, std::tuple<Ts...>
    >::type>::type type;
};

template <>
struct last_element<> {
  typedef void type;
};

} // namespace detail

/*******************************************************************************
 * Conversions from integral types to string types.
 ******************************************************************************/

/**
 * Returns the number of digits in the base 10 representation of an
 * uint64_t. Useful for preallocating buffers and such. It's also used
 * internally, see below. Measurements suggest that defining a
 * separate overload for 32-bit integers is not worthwhile.
 */

inline uint32_t digits10(uint64_t v) {
#ifdef __x86_64__

  // For this arch we can get a little help from specialized CPU instructions
  // which can count leading zeroes; 64 minus that is appx. log (base 2).
  // Use that to approximate base-10 digits (log_10) and then adjust if needed.

  // 10^i, defined for i 0 through 19.
  // This is 20 * 8 == 160 bytes, which fits neatly into 5 cache lines
  // (assuming a cache line size of 64).
  static const uint64_t powersOf10[20] RDD_ALIGNED(64) = {
    1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000,
    10000000000, 100000000000, 1000000000000, 10000000000000, 100000000000000,
    1000000000000000, 10000000000000000, 100000000000000000,
    1000000000000000000, 10000000000000000000UL
  };

  // "count leading zeroes" operation not valid; for 0; special case this.
  if UNLIKELY (! v) {
    return 1;
  }

  // bits is in the ballpark of log_2(v).
  const uint8_t leadingZeroes = __builtin_clzll(v);
  const auto bits = 63 - leadingZeroes;

  // approximate log_10(v) == log_10(2) * bits.
  // Integer magic below: 77/256 is appx. 0.3010 (log_10(2)).
  // The +1 is to make this the ceiling of the log_10 estimate.
  const uint32_t minLength = 1 + ((bits * 77) >> 8);

  // return that log_10 lower bound, plus adjust if input >= 10^(that bound)
  // in case there's a small error and we misjudged length.
  return minLength + (uint32_t) (UNLIKELY (v >= powersOf10[minLength]));

#else

  uint32_t result = 1;
  for (;;) {
    if (LIKELY(v < 10)) return result;
    if (LIKELY(v < 100)) return result + 1;
    if (LIKELY(v < 1000)) return result + 2;
    if (LIKELY(v < 10000)) return result + 3;
    // Skip ahead by 4 orders of magnitude
    v /= 10000U;
    result += 4;
  }

#endif
}

/**
 * Copies the ASCII base 10 representation of v into buffer and
 * returns the number of bytes written. Does NOT append a \0. Assumes
 * the buffer points to digits10(v) bytes of valid memory. Note that
 * uint64 needs at most 20 bytes, uint32_t needs at most 10 bytes,
 * uint16_t needs at most 5 bytes, and so on. Measurements suggest
 * that defining a separate overload for 32-bit integers is not
 * worthwhile.
 *
 * This primitive is unsafe because it makes the size assumption and
 * because it does not add a terminating \0.
 */

inline uint32_t uint64ToBufferUnsafe(uint64_t v, char *const buffer) {
  auto const result = digits10(v);
  // WARNING: using size_t or pointer arithmetic for pos slows down
  // the loop below 20x. This is because several 32-bit ops can be
  // done in parallel, but only fewer 64-bit ones.
  uint32_t pos = result - 1;
  while (v >= 10) {
    // Keep these together so a peephole optimization "sees" them and
    // computes them in one shot.
    auto const q = v / 10;
    auto const r = static_cast<uint32_t>(v % 10);
    buffer[pos--] = '0' + r;
    v = q;
  }
  // Last digit is trivial to handle
  buffer[pos] = static_cast<uint32_t>(v) + '0';
  return result;
}

/**
 * A single char gets appended.
 */
template <class Tgt>
void toAppend(char value, Tgt * result) {
  *result += value;
}

template<class T>
const typename std::enable_if<
  std::is_same<T, char>::value,
  size_t>::type
estimateSpaceNeeded(T) {
  return 1;
}

/**
 * Ubiquitous helper template for writing string appenders
 */
template <class T> struct IsSomeString {
  enum { value = std::is_same<T, std::string>::value };
};

/**
 * Everything implicitly convertible to const char* gets appended.
 */
template <class Tgt, class Src>
typename std::enable_if<
  std::is_convertible<Src, const char*>::value
  && IsSomeString<Tgt>::value>::type
toAppend(Src value, Tgt * result) {
  // Treat null pointers like an empty string, as in:
  // operator<<(std::ostream&, const char*).
  const char* c = value;
  if (c) {
    result->append(value);
  }
}

template<class Src>
typename std::enable_if<
  std::is_convertible<Src, const char*>::value,
  size_t>::type
estimateSpaceNeeded(Src value) {
  const char *c = value;
  if (c) {
    return rdd::StringPiece(value).size();
  };
  return 0;
}

template<class Src>
typename std::enable_if<
  (std::is_convertible<Src, rdd::StringPiece>::value ||
  IsSomeString<Src>::value) &&
  !std::is_convertible<Src, const char*>::value,
  size_t>::type
estimateSpaceNeeded(Src value) {
  return rdd::StringPiece(value).size();
}

template<class Src>
typename std::enable_if<
  std::is_pointer<Src>::value &&
  IsSomeString<std::remove_pointer<Src>>::value,
  size_t>::type
estimateSpaceNeeded(Src value) {
  return value->size();
}

/**
 * Strings get appended, too.
 */
template <class Tgt, class Src>
typename std::enable_if<
  IsSomeString<Src>::value && IsSomeString<Tgt>::value>::type
toAppend(const Src& value, Tgt * result) {
  result->append(value);
}

/**
 * and StringPiece objects too
 */
template <class Tgt>
typename std::enable_if<
   IsSomeString<Tgt>::value>::type
toAppend(StringPiece value, Tgt * result) {
  result->append(value.data(), value.size());
}

/**
 * int32_t and int64_t to string (by appending) go through here. The
 * result is APPENDED to a preexisting string passed as the second
 * parameter.
 */
template <class Tgt, class Src>
typename std::enable_if<
  std::is_integral<Src>::value && std::is_signed<Src>::value &&
  IsSomeString<Tgt>::value && sizeof(Src) >= 4>::type
toAppend(Src value, Tgt * result) {
  char buffer[20];
  if (value < 0) {
    result->push_back('-');
    result->append(buffer, uint64ToBufferUnsafe(-uint64_t(value), buffer));
  } else {
    result->append(buffer, uint64ToBufferUnsafe(value, buffer));
  }
}

template <class Src>
typename std::enable_if<
  std::is_integral<Src>::value && std::is_signed<Src>::value
  && sizeof(Src) >= 4 && sizeof(Src) < 16,
  size_t>::type
estimateSpaceNeeded(Src value) {
  if (value < 0) {
    return 1 + digits10(static_cast<uint64_t>(-value));
  }

  return digits10(static_cast<uint64_t>(value));
}

/**
 * As above, but for uint32_t and uint64_t.
 */
template <class Tgt, class Src>
typename std::enable_if<
  std::is_integral<Src>::value && !std::is_signed<Src>::value
  && IsSomeString<Tgt>::value && sizeof(Src) >= 4>::type
toAppend(Src value, Tgt * result) {
  char buffer[20];
  result->append(buffer, buffer + uint64ToBufferUnsafe(value, buffer));
}

template <class Src>
typename std::enable_if<
  std::is_integral<Src>::value && !std::is_signed<Src>::value
  && sizeof(Src) >= 4 && sizeof(Src) < 16,
  size_t>::type
estimateSpaceNeeded(Src value) {
  return digits10(value);
}

/**
 * All small signed and unsigned integers to string go through 32-bit
 * types int32_t and uint32_t, respectively.
 */
template <class Tgt, class Src>
typename std::enable_if<
  std::is_integral<Src>::value
  && IsSomeString<Tgt>::value && sizeof(Src) < 4>::type
toAppend(Src value, Tgt * result) {
  typedef typename
    std::conditional<std::is_signed<Src>::value, int64_t, uint64_t>::type
    Intermediate;
  toAppend<Tgt>(static_cast<Intermediate>(value), result);
}

template <class Src>
typename std::enable_if<
  std::is_integral<Src>::value
  && sizeof(Src) < 4
  && !std::is_same<Src, char>::value,
  size_t>::type
estimateSpaceNeeded(Src value) {
  typedef typename
    std::conditional<std::is_signed<Src>::value, int64_t, uint64_t>::type
    Intermediate;
  return estimateSpaceNeeded(static_cast<Intermediate>(value));
}

/**
 * Enumerated values get appended as integers.
 */
template <class Tgt, class Src>
typename std::enable_if<
  std::is_enum<Src>::value && IsSomeString<Tgt>::value>::type
toAppend(Src value, Tgt * result) {
  /* static */ if (Src(-1) < 0) {
    /* static */ if (sizeof(Src) <= sizeof(int)) {
      toAppend(static_cast<int>(value), result);
    } else {
      toAppend(static_cast<long>(value), result);
    }
  } else {
    /* static */ if (sizeof(Src) <= sizeof(int)) {
      toAppend(static_cast<unsigned int>(value), result);
    } else {
      toAppend(static_cast<unsigned long>(value), result);
    }
  }
}

template <class Src>
typename std::enable_if<
  std::is_enum<Src>::value, size_t>::type
estimateSpaceNeeded(Src value) {
  /* static */ if (Src(-1) < 0) {
    /* static */ if (sizeof(Src) <= sizeof(int)) {
      return estimateSpaceNeeded(static_cast<int>(value));
    } else {
      return estimateSpaceNeeded(static_cast<long>(value));
    }
  } else {
    /* static */ if (sizeof(Src) <= sizeof(int)) {
      return estimateSpaceNeeded(static_cast<unsigned int>(value));
    } else {
      return estimateSpaceNeeded(static_cast<unsigned long>(value));
    }
  }
}

/*******************************************************************************
 * Conversions from floating-point types to string types.
 ******************************************************************************/

namespace detail {
const int kConvMaxDecimalInShortestLow = -6;
const int kConvMaxDecimalInShortestHigh = 21;
} // rdd::detail

/** Wrapper around DoubleToStringConverter **/
template <class Tgt, class Src>
typename std::enable_if<
  std::is_floating_point<Src>::value
  && IsSomeString<Tgt>::value>::type
toAppend(
  Src value,
  Tgt * result,
  double_conversion::DoubleToStringConverter::DtoaMode mode,
  unsigned int numDigits) {
  using namespace double_conversion;
  DoubleToStringConverter
    conv(DoubleToStringConverter::NO_FLAGS,
         "Infinity", "NaN", 'E',
         detail::kConvMaxDecimalInShortestLow,
         detail::kConvMaxDecimalInShortestHigh,
         6,   // max leading padding zeros
         1);  // max trailing padding zeros
  char buffer[256];
  StringBuilder builder(buffer, sizeof(buffer));
  switch (mode) {
    case DoubleToStringConverter::SHORTEST:
      conv.ToShortest(value, &builder);
      break;
    case DoubleToStringConverter::FIXED:
      conv.ToFixed(value, numDigits, &builder);
      break;
    default:
      //CHECK(mode == DoubleToStringConverter::PRECISION);
      conv.ToPrecision(value, numDigits, &builder);
      break;
  }
  const size_t length = builder.position();
  builder.Finalize();
  result->append(buffer, length);
}

/**
 * As above, but for floating point
 */
template <class Tgt, class Src>
typename std::enable_if<
  std::is_floating_point<Src>::value
  && IsSomeString<Tgt>::value>::type
toAppend(Src value, Tgt * result) {
  toAppend(
    value, result, double_conversion::DoubleToStringConverter::SHORTEST, 0);
}

/**
 * Upper bound of the length of the output from
 * DoubleToStringConverter::ToShortest(double, StringBuilder*),
 * as used in toAppend(double, string*).
 */
template <class Src>
typename std::enable_if<
  std::is_floating_point<Src>::value, size_t>::type
estimateSpaceNeeded(Src value) {
  // kBase10MaximalLength is 17. We add 1 for decimal point,
  // e.g. 10.0/9 is 17 digits and 18 characters, including the decimal point.
  const int kMaxMantissaSpace =
    double_conversion::DoubleToStringConverter::kBase10MaximalLength + 1;
  // strlen("E-") + digits10(numeric_limits<double>::max_exponent10)
  const int kMaxExponentSpace = 2 + 3;
  static const int kMaxPositiveSpace = std::max(std::max(
      // E.g. 1.1111111111111111E-100.
      kMaxMantissaSpace + kMaxExponentSpace,
      // E.g. 0.000001.1111111111111111, if kConvMaxDecimalInShortestLow is -6.
      kMaxMantissaSpace - detail::kConvMaxDecimalInShortestLow),
      // If kConvMaxDecimalInShortestHigh is 21, then 1e21 is the smallest
      // number > 1 which ToShortest outputs in exponential notation,
      // so 21 is the longest non-exponential number > 1.
      detail::kConvMaxDecimalInShortestHigh
    );
  return kMaxPositiveSpace + (value < 0);  // +1 for minus sign, if negative
}

/**
 * This can be specialized, together with adding specialization
 * for estimateSpaceNeed for your type, so that we allocate
 * as much as you need instead of the default
 */
template<class Src>
struct HasLengthEstimator : std::false_type {};

template <class Src>
const typename std::enable_if<
  !std::is_fundamental<Src>::value
  && !IsSomeString<Src>::value
  && !std::is_convertible<Src, const char*>::value
  && !std::is_convertible<Src, StringPiece>::value
  && !std::is_enum<Src>::value
  && !HasLengthEstimator<Src>::value,
  size_t>::type
estimateSpaceNeeded(const Src&) {
  return sizeof(Src) + 1; // dumbest best effort ever?
}

namespace detail {

inline size_t estimateSpaceToReserve(size_t sofar) {
  return sofar;
}

template <class T, class... Ts>
size_t estimateSpaceToReserve(size_t sofar, const T& v, const Ts&... vs) {
  return estimateSpaceToReserve(sofar + estimateSpaceNeeded(v), vs...);
}

template<class T>
size_t estimateSpaceToReserve(size_t sofar, const T& v) {
  return sofar + estimateSpaceNeeded(v);
}

template<class...Ts>
void reserveInTarget(const Ts&...vs) {
  getLastElement(vs...)->reserve(estimateSpaceToReserve(0, vs...));
}

template<class Delimiter, class...Ts>
void reserveInTargetDelim(const Delimiter& d, const Ts&...vs) {
  static_assert(sizeof...(vs) >= 2, "Needs at least 2 args");
  size_t fordelim = (sizeof...(vs) - 2) * estimateSpaceToReserve(0, d);
  getLastElement(vs...)->reserve(estimateSpaceToReserve(fordelim, vs...));
}

/**
 * Variadic base case: append one element
 */
template <class T, class Tgt>
typename std::enable_if<
  IsSomeString<typename std::remove_pointer<Tgt>::type>
  ::value>::type
toAppendStrImpl(const T& v, Tgt result) {
  toAppend(v, result);
}

template <class T, class... Ts>
typename std::enable_if<sizeof...(Ts) >= 2
  && IsSomeString<
  typename std::remove_pointer<
    typename detail::last_element<Ts...>::type
  >::type>::value>::type
toAppendStrImpl(const T& v, const Ts&... vs) {
  toAppend(v, getLastElement(vs...));
  toAppendStrImpl(vs...);
}

template <class Delimiter, class T, class Tgt>
typename std::enable_if<
  IsSomeString<typename std::remove_pointer<Tgt>::type>
  ::value>::type
toAppendDelimStrImpl(const Delimiter& delim, const T& v, Tgt result) {
  toAppend(v, result);
}

template <class Delimiter, class T, class... Ts>
typename std::enable_if<sizeof...(Ts) >= 2
  && IsSomeString<
  typename std::remove_pointer<
    typename detail::last_element<Ts...>::type
  >::type>::value>::type
toAppendDelimStrImpl(const Delimiter& delim, const T& v, const Ts&... vs) {
  // we are really careful here, calling toAppend with just one element does
  // not try to estimate space needed (as we already did that). If we call
  // toAppend(v, delim, ....) we would do unnecesary size calculation
  toAppend(v, detail::getLastElement(vs...));
  toAppend(delim, detail::getLastElement(vs...));
  toAppendDelimStrImpl(delim, vs...);
}
} // rdd::detail


/**
 * Variadic conversion to string. Appends each element in turn.
 * If we have two or more things to append, we it will not reserve
 * the space for them and will depend on strings exponential growth.
 * If you just append once consider using toAppendFit which reserves
 * the space needed (but does not have exponential as a result).
 */
template <class... Ts>
typename std::enable_if<sizeof...(Ts) >= 3
  && IsSomeString<
  typename std::remove_pointer<
    typename detail::last_element<Ts...>::type
  >::type>::value>::type
toAppend(const Ts&... vs) {
  ::rdd::detail::toAppendStrImpl(vs...);
}

/**
 * Special version of the call that preallocates exaclty as much memory
 * as need for arguments to be stored in target. This means we are
 * not doing exponential growth when we append. If you are using it
 * in a loop you are aiming at your foot with a big perf-destroying
 * bazooka.
 * On the other hand if you are appending to a string once, this
 * will probably save a few calls to malloc.
 */
template <class... Ts>
typename std::enable_if<
  IsSomeString<
  typename std::remove_pointer<
    typename detail::last_element<Ts...>::type
  >::type>::value>::type
toAppendFit(const Ts&... vs) {
  ::rdd::detail::reserveInTarget(vs...);
  toAppend(vs...);
}

template <class Ts>
void toAppendFit(const Ts&) {}

/**
 * Variadic base case: do nothing.
 */
template <class Tgt>
typename std::enable_if<IsSomeString<Tgt>::value>::type
toAppend(Tgt* result) {
}

/**
 * Variadic base case: do nothing.
 */
template <class Delimiter, class Tgt>
typename std::enable_if<IsSomeString<Tgt>::value>::type
toAppendDelim(const Delimiter& delim, Tgt* result) {
}

/**
 * 1 element: same as toAppend.
 */
template <class Delimiter, class T, class Tgt>
typename std::enable_if<IsSomeString<Tgt>::value>::type
toAppendDelim(const Delimiter& delim, const T& v, Tgt* tgt) {
  toAppend(v, tgt);
}

/**
 * Append to string with a delimiter in between elements. Check out
 * comments for toAppend for details about memory allocation.
 */
template <class Delimiter, class... Ts>
typename std::enable_if<sizeof...(Ts) >= 3
  && IsSomeString<
  typename std::remove_pointer<
    typename detail::last_element<Ts...>::type
  >::type>::value>::type
toAppendDelim(const Delimiter& delim, const Ts&... vs) {
  detail::toAppendDelimStrImpl(delim, vs...);
}

/**
 * Detail in comment for toAppendFit
 */
template <class Delimiter, class... Ts>
typename std::enable_if<
  IsSomeString<
  typename std::remove_pointer<
    typename detail::last_element<Ts...>::type
  >::type>::value>::type
toAppendDelimFit(const Delimiter& delim, const Ts&... vs) {
  detail::reserveInTargetDelim(delim, vs...);
  toAppendDelim(delim, vs...);
}

template <class De, class Ts>
void toAppendDelimFit(const De&, const Ts&) {}

/**
 * to<SomeString>(v1, v2, ...) uses toAppend() (see below) as back-end
 * for all types.
 */
template <class Tgt, class... Ts>
typename std::enable_if<
  IsSomeString<Tgt>::value && (
    sizeof...(Ts) != 1 ||
    !std::is_same<Tgt, typename detail::last_element<Ts...>::type>::value),
  Tgt>::type
to(const Ts&... vs) {
  Tgt result;
  toAppendFit(vs..., &result);
  return result;
}

/**
 * toDelim<SomeString>(SomeString str) returns itself.
 */
template <class Tgt, class Delim, class Src>
typename std::enable_if<
  IsSomeString<Tgt>::value && std::is_same<Tgt, Src>::value,
  Tgt>::type
toDelim(const Delim& delim, const Src & value) {
  return value;
}

/**
 * toDelim<SomeString>(delim, v1, v2, ...) uses toAppendDelim() as
 * back-end for all types.
 */
template <class Tgt, class Delim, class... Ts>
typename std::enable_if<
  IsSomeString<Tgt>::value && (
    sizeof...(Ts) != 1 ||
    !std::is_same<Tgt, typename detail::last_element<Ts...>::type>::value),
  Tgt>::type
toDelim(const Delim& delim, const Ts&... vs) {
  Tgt result;
  toAppendDelimFit(delim, vs..., &result);
  return result;
}

/*******************************************************************************
 * Conversions from string types to integral types.
 ******************************************************************************/

namespace detail {

/**
 * Finds the first non-digit in a string. The number of digits
 * searched depends on the precision of the Tgt integral. Assumes the
 * string starts with NO whitespace and NO sign.
 *
 * The semantics of the routine is:
 *   for (;; ++b) {
 *     if (b >= e || !isdigit(*b)) return b;
 *   }
 *
 *  Complete unrolling marks bottom-line (i.e. entire conversion)
 *  improvements of 20%.
 */
  template <class Tgt>
  const char* findFirstNonDigit(const char* b, const char* e) {
    for (; b < e; ++b) {
      auto const c = static_cast<unsigned>(*b) - '0';
      if (c >= 10) break;
    }
    return b;
  }

  // Maximum value of number when represented as a string
  template <class T> struct MaxString {
    static const char*const value;
  };


/*
 * Lookup tables that converts from a decimal character value to an integral
 * binary value, shifted by a decimal "shift" multiplier.
 * For all character values in the range '0'..'9', the table at those
 * index locations returns the actual decimal value shifted by the multiplier.
 * For all other values, the lookup table returns an invalid OOR value.
 */
// Out-of-range flag value, larger than the largest value that can fit in
// four decimal bytes (9999), but four of these added up together should
// still not overflow uint16_t.
const int32_t OOR = 10000;

RDD_ALIGNED(16) const uint16_t shift1[] = {
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 0-9
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  10
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  20
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  30
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, 0,         //  40
  1, 2, 3, 4, 5, 6, 7, 8, 9, OOR, OOR,
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  60
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  70
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  80
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  90
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 100
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 110
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 120
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 130
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 140
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 150
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 160
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 170
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 180
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 190
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 200
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 210
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 220
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 230
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 240
  OOR, OOR, OOR, OOR, OOR, OOR                       // 250
};

RDD_ALIGNED(16) const uint16_t shift10[] = {
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 0-9
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  10
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  20
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  30
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, 0,         //  40
  10, 20, 30, 40, 50, 60, 70, 80, 90, OOR, OOR,
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  60
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  70
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  80
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  90
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 100
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 110
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 120
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 130
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 140
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 150
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 160
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 170
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 180
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 190
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 200
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 210
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 220
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 230
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 240
  OOR, OOR, OOR, OOR, OOR, OOR                       // 250
};

RDD_ALIGNED(16) const uint16_t shift100[] = {
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 0-9
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  10
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  20
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  30
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, 0,         //  40
  100, 200, 300, 400, 500, 600, 700, 800, 900, OOR, OOR,
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  60
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  70
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  80
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  90
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 100
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 110
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 120
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 130
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 140
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 150
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 160
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 170
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 180
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 190
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 200
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 210
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 220
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 230
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 240
  OOR, OOR, OOR, OOR, OOR, OOR                       // 250
};

RDD_ALIGNED(16) const uint16_t shift1000[] = {
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 0-9
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  10
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  20
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  30
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, 0,         //  40
  1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, OOR, OOR,
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  60
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  70
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  80
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  //  90
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 100
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 110
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 120
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 130
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 140
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 150
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 160
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 170
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 180
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 190
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 200
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 210
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 220
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 230
  OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR, OOR,  // 240
  OOR, OOR, OOR, OOR, OOR, OOR                       // 250
};

/**
 * String represented as a pair of pointers to char to unsigned
 * integrals. Assumes NO whitespace before or after, and also that the
 * string is composed entirely of digits. Tgt must be unsigned, and no
 * sign is allowed in the string (even it's '+'). String may be empty,
 * in which case digits_to throws.
 */
  template <class Tgt>
  Tgt digits_to(const char * b, const char * e) {

    static_assert(!std::is_signed<Tgt>::value, "Unsigned type expected");
    assert(b <= e);

    const size_t size = e - b;

    /* Although the string is entirely made of digits, we still need to
     * check for overflow.
     */
    if (size >= std::numeric_limits<Tgt>::digits10 + 1) {
      // Leading zeros? If so, recurse to keep things simple
      if (b < e && *b == '0') {
        for (++b;; ++b) {
          if (b == e) return 0; // just zeros, e.g. "0000"
          if (*b != '0') return digits_to<Tgt>(b, e);
        }
      }
      RDD_RANGE_CHECK_BEGIN_END(
        size == std::numeric_limits<Tgt>::digits10 + 1 &&
        strncmp(b, detail::MaxString<Tgt>::value, size) <= 0,
        "Numeric overflow upon conversion", b, e);
    }

    // Here we know that the number won't overflow when
    // converted. Proceed without checks.

    Tgt result = 0;

    for (; e - b >= 4; b += 4) {
      result *= 10000;
      const int32_t r0 = shift1000[static_cast<size_t>(b[0])];
      const int32_t r1 = shift100[static_cast<size_t>(b[1])];
      const int32_t r2 = shift10[static_cast<size_t>(b[2])];
      const int32_t r3 = shift1[static_cast<size_t>(b[3])];
      const auto sum = r0 + r1 + r2 + r3;
      assert(sum < OOR && "Assumption: string only has digits");
      result += sum;
    }

    switch (e - b) {
      case 3: {
        const int32_t r0 = shift100[static_cast<size_t>(b[0])];
        const int32_t r1 = shift10[static_cast<size_t>(b[1])];
        const int32_t r2 = shift1[static_cast<size_t>(b[2])];
        const auto sum = r0 + r1 + r2;
        assert(sum < OOR && "Assumption: string only has digits");
        return result * 1000 + sum;
      }
      case 2: {
        const int32_t r0 = shift10[static_cast<size_t>(b[0])];
        const int32_t r1 = shift1[static_cast<size_t>(b[1])];
        const auto sum = r0 + r1;
        assert(sum < OOR && "Assumption: string only has digits");
        return result * 100 + sum;
      }
      case 1: {
        const int32_t sum = shift1[static_cast<size_t>(b[0])];
        assert(sum < OOR && "Assumption: string only has digits");
        return result * 10 + sum;
      }
    }

    assert(b == e);
    RDD_RANGE_CHECK_BEGIN_END(size > 0,
                              "Found no digits to convert in input", b, e);
    return result;
  }


  bool str_to_bool(StringPiece * src);

}                                 // namespace detail

/**
 * String represented as a pair of pointers to char to unsigned
 * integrals. Assumes NO whitespace before or after.
 */
template <class Tgt>
typename std::enable_if<
  std::is_integral<Tgt>::value && !std::is_signed<Tgt>::value
  && !std::is_same<typename std::remove_cv<Tgt>::type, bool>::value,
  Tgt>::type
to(const char * b, const char * e) {
  return detail::digits_to<Tgt>(b, e);
}

/**
 * String represented as a pair of pointers to char to signed
 * integrals. Assumes NO whitespace before or after. Allows an
 * optional leading sign.
 */
template <class Tgt>
typename std::enable_if<
  std::is_integral<Tgt>::value && std::is_signed<Tgt>::value,
  Tgt>::type
to(const char * b, const char * e) {
  RDD_RANGE_CHECK(b < e, "Empty input string in conversion to integral",
                  to<std::string>("b: ", intptr_t(b), " e: ", intptr_t(e)));
  if (!isdigit(*b)) {
    if (*b == '-') {
      Tgt result = -to<typename std::make_unsigned<Tgt>::type>(b + 1, e);
      RDD_RANGE_CHECK_BEGIN_END(result <= 0, "Negative overflow.", b, e);
      return result;
    }
    RDD_RANGE_CHECK_BEGIN_END(*b == '+', "Invalid lead character", b, e);
    ++b;
  }
  Tgt result = to<typename std::make_unsigned<Tgt>::type>(b, e);
  RDD_RANGE_CHECK_BEGIN_END(result >= 0, "Overflow", b, e);
  return result;
}

/**
 * Parsing strings to integrals. These routines differ from
 * to<integral>(string) in that they take a POINTER TO a StringPiece
 * and alter that StringPiece to reflect progress information.
 */

/**
 * StringPiece to integrals, with progress information. Alters the
 * StringPiece parameter to munch the already-parsed characters.
 */
template <class Tgt>
typename std::enable_if<
  std::is_integral<Tgt>::value
  && !std::is_same<typename std::remove_cv<Tgt>::type, bool>::value,
  Tgt>::type
to(StringPiece * src) {

  auto b = src->data(), past = src->data() + src->size();
  for (;; ++b) {
    RDD_RANGE_CHECK_STRINGPIECE(b < past,
                                "No digits found in input string", *src);
    if (!isspace(*b)) break;
  }

  auto m = b;

  // First digit is customized because we test for sign
  bool negative = false;
  /* static */ if (std::is_signed<Tgt>::value) {
    if (!isdigit(*m)) {
      if (*m == '-') {
        negative = true;
      } else {
        RDD_RANGE_CHECK_STRINGPIECE(*m == '+', "Invalid leading character in "
                                    "conversion to integral", *src);
      }
      ++b;
      ++m;
    }
  }
  RDD_RANGE_CHECK_STRINGPIECE(m < past, "No digits found in input string",
                              *src);
  RDD_RANGE_CHECK_STRINGPIECE(isdigit(*m), "Non-digit character found", *src);
  m = detail::findFirstNonDigit<Tgt>(m + 1, past);

  Tgt result;
  /* static */ if (!std::is_signed<Tgt>::value) {
    result = detail::digits_to<typename std::make_unsigned<Tgt>::type>(b, m);
  } else {
    auto t = detail::digits_to<typename std::make_unsigned<Tgt>::type>(b, m);
    if (negative) {
      result = -t;
      RDD_RANGE_CHECK_STRINGPIECE(result <= 0, "Negative overflow", *src);
    } else {
      result = t;
      RDD_RANGE_CHECK_STRINGPIECE(result >= 0, "Overflow", *src);
    }
  }
  src->advance(m - src->data());
  return result;
}

/**
 * StringPiece to bool, with progress information. Alters the
 * StringPiece parameter to munch the already-parsed characters.
 */
template <class Tgt>
typename std::enable_if<
  std::is_same<typename std::remove_cv<Tgt>::type, bool>::value,
  Tgt>::type
to(StringPiece * src) {
  return detail::str_to_bool(src);
}

namespace detail {

/**
 * Enforce that the suffix following a number is made up only of whitespace.
 */
inline void enforceWhitespace(const char* b, const char* e) {
  for (; b != e; ++b) {
    RDD_RANGE_CHECK_BEGIN_END(isspace(*b),
                              to<std::string>("Non-whitespace: ", *b),
                                b, e);
  }
}

}  // namespace detail

/**
 * String or StringPiece to integrals. Accepts leading and trailing
 * whitespace, but no non-space trailing characters.
 */
template <class Tgt>
typename std::enable_if<
  std::is_integral<Tgt>::value,
  Tgt>::type
to(StringPiece src) {
  Tgt result = to<Tgt>(&src);
  detail::enforceWhitespace(src.data(), src.data() + src.size());
  return result;
}

/*******************************************************************************
 * Conversions from string types to floating-point types.
 ******************************************************************************/

/**
 * StringPiece to double, with progress information. Alters the
 * StringPiece parameter to munch the already-parsed characters.
 */
template <class Tgt>
inline typename std::enable_if<
  std::is_floating_point<Tgt>::value,
  Tgt>::type
to(StringPiece *const src) {
  using namespace double_conversion;
  static StringToDoubleConverter
    conv(StringToDoubleConverter::ALLOW_TRAILING_JUNK
         | StringToDoubleConverter::ALLOW_LEADING_SPACES,
         0.0,
         // return this for junk input string
         std::numeric_limits<double>::quiet_NaN(),
         nullptr, nullptr);

  RDD_RANGE_CHECK_STRINGPIECE(!src->empty(),
                              "No digits found in input string", *src);

  int length;
  auto result = conv.StringToDouble(src->data(),
                                    static_cast<int>(src->size()),
                                    &length); // processed char count

  if (!std::isnan(result)) {
    src->advance(length);
    return result;
  }

  for (;; src->advance(1)) {
    if (src->empty()) {
      throw std::range_error("Unable to convert an empty string"
                             " to a floating point value.");
    }
    if (!isspace(src->front())) {
      break;
    }
  }

  // Was that "inf[inity]"?
  if (src->size() >= 3 && toupper((*src)[0]) == 'I'
        && toupper((*src)[1]) == 'N' && toupper((*src)[2]) == 'F') {
    if (src->size() >= 8 &&
        toupper((*src)[3]) == 'I' &&
        toupper((*src)[4]) == 'N' &&
        toupper((*src)[5]) == 'I' &&
        toupper((*src)[6]) == 'T' &&
        toupper((*src)[7]) == 'Y') {
      src->advance(8);
    } else {
      src->advance(3);
    }
    return std::numeric_limits<Tgt>::infinity();
  }

  // Was that "-inf[inity]"?
  if (src->size() >= 4 && toupper((*src)[0]) == '-'
      && toupper((*src)[1]) == 'I' && toupper((*src)[2]) == 'N'
      && toupper((*src)[3]) == 'F') {
    if (src->size() >= 9 &&
        toupper((*src)[4]) == 'I' &&
        toupper((*src)[5]) == 'N' &&
        toupper((*src)[6]) == 'I' &&
        toupper((*src)[7]) == 'T' &&
        toupper((*src)[8]) == 'Y') {
      src->advance(9);
    } else {
      src->advance(4);
    }
    return -std::numeric_limits<Tgt>::infinity();
  }

  // "nan"?
  if (src->size() >= 3 && toupper((*src)[0]) == 'N'
        && toupper((*src)[1]) == 'A' && toupper((*src)[2]) == 'N') {
    src->advance(3);
    return std::numeric_limits<Tgt>::quiet_NaN();
  }

  // "-nan"?
  if (src->size() >= 4 &&
      toupper((*src)[0]) == '-' &&
      toupper((*src)[1]) == 'N' &&
      toupper((*src)[2]) == 'A' &&
      toupper((*src)[3]) == 'N') {
    src->advance(4);
    return -std::numeric_limits<Tgt>::quiet_NaN();
  }

  // All bets are off
  throw std::range_error("Unable to convert \"" + src->toString()
                         + "\" to a floating point value.");
}

/**
 * Any string, const char*, or StringPiece to double.
 */
template <class Tgt>
typename std::enable_if<
  std::is_floating_point<Tgt>::value,
  Tgt>::type
to(StringPiece src) {
  Tgt result = to<double>(&src);
  detail::enforceWhitespace(src.data(), src.data() + src.size());
  return result;
}

/*******************************************************************************
 * Integral to floating point and back
 ******************************************************************************/

/**
 * Checked conversion from integral to flating point and back. The
 * result must be convertible back to the source type without loss of
 * precision. This seems Draconian but sometimes is what's needed, and
 * complements existing routines nicely. For various rounding
 * routines, see <math>.
 */
template <class Tgt, class Src>
typename std::enable_if<
  (std::is_integral<Src>::value && std::is_floating_point<Tgt>::value)
  ||
  (std::is_floating_point<Src>::value && std::is_integral<Tgt>::value),
  Tgt>::type
to(const Src & value) {
  Tgt result = value;
  auto witness = static_cast<Src>(result);
  if (value != witness) {
    throw std::range_error(
      to<std::string>("to<>: loss of precision when converting ", value,
                      " to other type").c_str());
  }
  return result;
}

/*******************************************************************************
 * Enum to anything and back
 ******************************************************************************/

template <class Tgt, class Src>
typename std::enable_if<
  std::is_enum<Src>::value && !std::is_same<Src, Tgt>::value, Tgt>::type
to(const Src & value) {
  /* static */ if (Src(-1) < 0) {
    /* static */ if (sizeof(Src) <= sizeof(int)) {
      return to<Tgt>(static_cast<int>(value));
    } else {
      return to<Tgt>(static_cast<long>(value));
    }
  } else {
    /* static */ if (sizeof(Src) <= sizeof(int)) {
      return to<Tgt>(static_cast<unsigned int>(value));
    } else {
      return to<Tgt>(static_cast<unsigned long>(value));
    }
  }
}

template <class Tgt, class Src>
typename std::enable_if<
  std::is_enum<Tgt>::value && !std::is_same<Src, Tgt>::value, Tgt>::type
to(const Src & value) {
  /* static */ if (Tgt(-1) < 0) {
    /* static */ if (sizeof(Tgt) <= sizeof(int)) {
      return static_cast<Tgt>(to<int>(value));
    } else {
      return static_cast<Tgt>(to<long>(value));
    }
  } else {
    /* static */ if (sizeof(Tgt) <= sizeof(int)) {
      return static_cast<Tgt>(to<unsigned int>(value));
    } else {
      return static_cast<Tgt>(to<unsigned long>(value));
    }
  }
}

} // namespace rdd

// RDD_CONV_INTERNAL is defined by Conv.cpp.  Keep the RDD_RANGE_CHECK
// macro for use in Conv.cpp, but #undefine it everywhere else we are included,
// to avoid defining this global macro name in other files that include Conv.h.
#ifndef RDD_CONV_INTERNAL
#undef RDD_RANGE_CHECK
#undef RDD_RANGE_CHECK_BEGIN_END
#undef RDD_RANGE_CHECK_STRINGPIECE
#endif
