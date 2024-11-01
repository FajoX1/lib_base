// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once
#pragma warning(push)
#pragma warning(disable : 4996)

#include "base/bytes.h"
#include "base/algorithm.h"
#include "base/assertion.h"
#include "base/basic_types.h"

extern "C" {
#include <openssl/bn.h>
#include <openssl/sha.h>
#include <openssl/aes.h>
#include <openssl/modes.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
} // extern "C"

#ifdef small
#undef small
#endif // small

namespace openssl {

class Context {
public:
	Context() : _data(BN_CTX_new()) {
	}
	Context(const Context &other) = delete;
	Context(Context &&other) : _data(base::take(other._data)) {
	}
	Context &operator=(const Context &other) = delete;
	Context &operator=(Context &&other) {
		_data = base::take(other._data);
		return *this;
	}
	~Context() {
		if (_data) {
			BN_CTX_free(_data);
		}
	}

	BN_CTX *raw() const {
		return _data;
	}

private:
	BN_CTX *_data = nullptr;

};

class BigNum {
public:
	BigNum() = default;
	BigNum(const BigNum &other)
	: _data((other.failed() || other.isZero())
		? nullptr
		: BN_dup(other.raw()))
	, _failed(other._failed) {
	}
	BigNum(BigNum &&other)
	: _data(std::exchange(other._data, nullptr))
	, _failed(std::exchange(other._failed, false)) {
	}
	BigNum &operator=(const BigNum &other) {
		if (other.failed()) {
			_failed = true;
		} else if (other.isZero()) {
			clear();
			_failed = false;
		} else if (!_data) {
			_data = BN_dup(other.raw());
			_failed = false;
		} else {
			_failed = !BN_copy(raw(), other.raw());
		}
		return *this;
	}
	BigNum &operator=(BigNum &&other) {
		std::swap(_data, other._data);
		std::swap(_failed, other._failed);
		return *this;
	}
	~BigNum() {
		clear();
	}

	explicit BigNum(unsigned int word) : BigNum() {
		setWord(word);
	}
	explicit BigNum(bytes::const_span bytes) : BigNum() {
		setBytes(bytes);
	}

	BigNum &setWord(unsigned int word) {
		if (!word) {
			clear();
			_failed = false;
		} else {
			_failed = !BN_set_word(raw(), word);
		}
		return *this;
	}
	BigNum &setBytes(bytes::const_span bytes) {
		if (bytes.empty()) {
			clear();
			_failed = false;
		} else {
			_failed = !BN_bin2bn(
				reinterpret_cast<const unsigned char*>(bytes.data()),
				bytes.size(),
				raw());
		}
		return *this;
	}

	BigNum &setAdd(const BigNum &a, const BigNum &b) {
		if (a.failed() || b.failed()) {
			_failed = true;
		} else {
			_failed = !BN_add(raw(), a.raw(), b.raw());
		}
		return *this;
	}
	BigNum &setSub(const BigNum &a, const BigNum &b) {
		if (a.failed() || b.failed()) {
			_failed = true;
		} else {
			_failed = !BN_sub(raw(), a.raw(), b.raw());
		}
		return *this;
	}
	BigNum &setMul(
			const BigNum &a,
			const BigNum &b,
			const Context &context = Context()) {
		if (a.failed() || b.failed()) {
			_failed = true;
		} else {
			_failed = !BN_mul(raw(), a.raw(), b.raw(), context.raw());
		}
		return *this;
	}
	BigNum &setModAdd(
			const BigNum &a,
			const BigNum &b,
			const BigNum &m,
			const Context &context = Context()) {
		if (a.failed() || b.failed() || m.failed()) {
			_failed = true;
		} else if (a.isNegative() || b.isNegative() || m.isNegative()) {
			_failed = true;
		} else if (!BN_mod_add(raw(), a.raw(), b.raw(), m.raw(), context.raw())) {
			_failed = true;
		} else if (isNegative()) {
			_failed = true;
		} else {
			_failed = false;
		}
		return *this;
	}
	BigNum &setModSub(
			const BigNum &a,
			const BigNum &b,
			const BigNum &m,
			const Context &context = Context()) {
		if (a.failed() || b.failed() || m.failed()) {
			_failed = true;
		} else if (a.isNegative() || b.isNegative() || m.isNegative()) {
			_failed = true;
		} else if (!BN_mod_sub(raw(), a.raw(), b.raw(), m.raw(), context.raw())) {
			_failed = true;
		} else if (isNegative()) {
			_failed = true;
		} else {
			_failed = false;
		}
		return *this;
	}
	BigNum &setModMul(
			const BigNum &a,
			const BigNum &b,
			const BigNum &m,
			const Context &context = Context()) {
		if (a.failed() || b.failed() || m.failed()) {
			_failed = true;
		} else if (a.isNegative() || b.isNegative() || m.isNegative()) {
			_failed = true;
		} else if (!BN_mod_mul(raw(), a.raw(), b.raw(), m.raw(), context.raw())) {
			_failed = true;
		} else if (isNegative()) {
			_failed = true;
		} else {
			_failed = false;
		}
		return *this;
	}
	BigNum &setModInverse(
			const BigNum &a,
			const BigNum &m,
			const Context &context = Context()) {
		if (a.failed() || m.failed()) {
			_failed = true;
		} else if (a.isNegative() || m.isNegative()) {
			_failed = true;
		} else if (!BN_mod_inverse(raw(), a.raw(), m.raw(), context.raw())) {
			_failed = true;
		} else if (isNegative()) {
			_failed = true;
		} else {
			_failed = false;
		}
		return *this;
	}
	BigNum &setModExp(
			const BigNum &base,
			const BigNum &power,
			const BigNum &m,
			const Context &context = Context()) {
		if (base.failed() || power.failed() || m.failed()) {
			_failed = true;
		} else if (base.isNegative() || power.isNegative() || m.isNegative()) {
			_failed = true;
		} else if (!BN_mod_exp(raw(), base.raw(), power.raw(), m.raw(), context.raw())) {
			_failed = true;
		} else if (isNegative()) {
			_failed = true;
		} else {
			_failed = false;
		}
		return *this;
	}
	BigNum &setGcd(
			const BigNum &a,
			const BigNum &b,
			const Context &context = Context()) {
		if (a.failed() || b.failed()) {
			_failed = true;
		} else if (a.isNegative() || b.isNegative()) {
			_failed = true;
		} else if (!BN_gcd(raw(), a.raw(), b.raw(), context.raw())) {
			_failed = true;
		} else if (isNegative()) {
			_failed = true;
		} else {
			_failed = false;
		}
		return *this;
	}

	[[nodiscard]] bool isZero() const {
		return !failed() && (!_data || BN_is_zero(raw()));
	}

	[[nodiscard]] bool isOne() const {
		return !failed() && _data && BN_is_one(raw());
	}

	[[nodiscard]] bool isNegative() const {
		return !failed() && _data && BN_is_negative(raw());
	}

	[[nodiscard]] bool isPrime(const Context &context = Context()) const {
    		if (failed() || !_data) {
        		return false;
    		}
    		// constexpr auto kMillerRabinIterationCount = 64;
    		const auto result = BN_check_prime(
        		raw(),
        		context.raw(),
        		nullptr
		);

    		if (result == 1) {
    		    return true; 
    		} else if (result == 0) {
    		    return false; 
    		} else {
    		    _failed = true; 
    		}
   		return false; 
	}

	BigNum &subWord(unsigned int word) {
		if (failed()) {
			return *this;
		} else if (!BN_sub_word(raw(), word)) {
			_failed = true;
		}
		return *this;
	}
	BigNum &divWord(BN_ULONG word, BN_ULONG *mod = nullptr) {
		Expects(word != 0);

		const auto result = failed()
			? (BN_ULONG)-1
			: BN_div_word(raw(), word);
		if (result == (BN_ULONG)-1) {
			_failed = true;
		}
		if (mod) {
			*mod = result;
		}
		return *this;
	}
	[[nodiscard]] BN_ULONG countModWord(BN_ULONG word) const {
		Expects(word != 0);

		return failed() ? (BN_ULONG)-1 : BN_mod_word(raw(), word);
	}

	[[nodiscard]] int bitsSize() const {
		return failed() ? 0 : BN_num_bits(raw());
	}
	[[nodiscard]] int bytesSize() const {
		return failed() ? 0 : BN_num_bytes(raw());
	}

	[[nodiscard]] bytes::vector getBytes() const {
		if (failed()) {
			return {};
		}
		auto length = BN_num_bytes(raw());
		auto result = bytes::vector(length);
		auto resultSize = BN_bn2bin(
			raw(),
			reinterpret_cast<unsigned char*>(result.data()));
		Assert(resultSize == length);
		return result;
	}

	[[nodiscard]] BIGNUM *raw() {
		if (!_data) _data = BN_new();
		return _data;
	}
	[[nodiscard]] const BIGNUM *raw() const {
		if (!_data) _data = BN_new();
		return _data;
	}
	[[nodiscard]] BIGNUM *takeRaw() {
		return _failed
			? nullptr
			: _data
			? std::exchange(_data, nullptr)
			: BN_new();
	}

	[[nodiscard]] bool failed() const {
		return _failed;
	}

	[[nodiscard]] static BigNum Add(const BigNum &a, const BigNum &b) {
		return BigNum().setAdd(a, b);
	}
	[[nodiscard]] static BigNum Sub(const BigNum &a, const BigNum &b) {
		return BigNum().setSub(a, b);
	}
	[[nodiscard]] static BigNum Mul(
			const BigNum &a,
			const BigNum &b,
			const Context &context = Context()) {
		return BigNum().setMul(a, b, context);
	}
	[[nodiscard]] static BigNum ModAdd(
			const BigNum &a,
			const BigNum &b,
			const BigNum &mod,
			const Context &context = Context()) {
		return BigNum().setModAdd(a, b, mod, context);
	}
	[[nodiscard]] static BigNum ModSub(
			const BigNum &a,
			const BigNum &b,
			const BigNum &mod,
			const Context &context = Context()) {
		return BigNum().setModSub(a, b, mod, context);
	}
	[[nodiscard]] static BigNum ModMul(
			const BigNum &a,
			const BigNum &b,
			const BigNum &mod,
			const Context &context = Context()) {
		return BigNum().setModMul(a, b, mod, context);
	}
	[[nodiscard]] static BigNum ModInverse(
			const BigNum &a,
			const BigNum &mod,
			const Context &context = Context()) {
		return BigNum().setModInverse(a, mod, context);
	}
	[[nodiscard]] static BigNum ModExp(
			const BigNum &base,
			const BigNum &power,
			const BigNum &mod,
			const Context &context = Context()) {
		return BigNum().setModExp(base, power, mod, context);
	}
	[[nodiscard]] static int Compare(const BigNum &a, const BigNum &b) {
		return a.failed() ? -1 : b.failed() ? 1 : BN_cmp(a.raw(), b.raw());
	}
	static void Div(
			BigNum *dv,
			BigNum *rem,
			const BigNum &a,
			const BigNum &b,
			const Context &context = Context()) {
		if (!dv && !rem) {
			return;
		} else if (a.failed()
			|| b.failed()
			|| !BN_div(
				dv ? dv->raw() : nullptr,
				rem ? rem->raw() : nullptr,
				a.raw(),
				b.raw(),
				context.raw())) {
			if (dv) {
				dv->_failed = true;
			}
			if (rem) {
				rem->_failed = true;
			}
		} else {
			if (dv) {
				dv->_failed = false;
			}
			if (rem) {
				rem->_failed = false;
			}
		}
	}
	[[nodiscard]] static BigNum Failed() {
		auto result = BigNum();
		result._failed = true;
		return result;
	}

private:
	void clear() {
		BN_clear_free(std::exchange(_data, nullptr));
	}

	mutable BIGNUM *_data = nullptr;
	mutable bool _failed = false;

};

namespace details {

template <typename Context, typename Method, typename Arg>
inline void ShaUpdate(Context context, Method method, Arg &&arg) {
	const auto span = bytes::make_span(arg);
	method(context, span.data(), span.size());
}

template <typename Context, typename Method, typename Arg, typename ...Args>
inline void ShaUpdate(Context context, Method method, Arg &&arg, Args &&...args) {
	const auto span = bytes::make_span(arg);
	method(context, span.data(), span.size());
	ShaUpdate(context, method, args...);
}

template <size_type Size, typename Method>
inline void Sha(
		bytes::span dst,
		Method method,
		bytes::const_span data) {
	Expects(dst.size() >= Size);

	method(
		reinterpret_cast<const unsigned char*>(data.data()),
		data.size(),
		reinterpret_cast<unsigned char*>(dst.data()));
}

template <size_type Size, typename Method>
[[nodiscard]] inline bytes::vector Sha(
		Method method,
		bytes::const_span data) {
	auto bytes = bytes::vector(Size);
	Sha<Size>(bytes, method, data);
	return bytes;
}

template <
	size_type Size,
	typename Context,
	typename Init,
	typename Update,
	typename Finalize,
	typename ...Args,
	typename = std::enable_if_t<(sizeof...(Args) > 1)>>
[[nodiscard]] bytes::vector Sha(
		Context context,
		Init init,
		Update update,
		Finalize finalize,
		Args &&...args) {
	auto bytes = bytes::vector(Size);

	init(&context);
	ShaUpdate(&context, update, args...);
	finalize(reinterpret_cast<unsigned char*>(bytes.data()), &context);

	return bytes;
}

template <
	size_type Size,
	typename Evp>
[[nodiscard]] bytes::vector Pbkdf2(
		bytes::const_span password,
		bytes::const_span salt,
		int iterations,
		Evp evp) {
	auto result = bytes::vector(Size);
	PKCS5_PBKDF2_HMAC(
		reinterpret_cast<const char*>(password.data()),
		password.size(),
		reinterpret_cast<const unsigned char*>(salt.data()),
		salt.size(),
		iterations,
		evp,
		result.size(),
		reinterpret_cast<unsigned char*>(result.data()));
	return result;
}

} // namespace details

constexpr auto kSha1Size = size_type(SHA_DIGEST_LENGTH);
constexpr auto kSha256Size = size_type(SHA256_DIGEST_LENGTH);
constexpr auto kSha512Size = size_type(SHA512_DIGEST_LENGTH);

[[nodiscard]] inline bytes::vector Sha1(bytes::const_span data) {
	return details::Sha<kSha1Size>(SHA1, data);
}

inline void Sha1To(bytes::span dst, bytes::const_span data) {
	details::Sha<kSha1Size>(dst, SHA1, data);
}

template <
	typename ...Args,
	typename = std::enable_if_t<(sizeof...(Args) > 1)>>
[[nodiscard]] inline bytes::vector Sha1(Args &&...args) {
	return details::Sha<kSha1Size>(
		SHA_CTX(),
		SHA1_Init,
		SHA1_Update,
		SHA1_Final,
		args...);
}

[[nodiscard]] inline bytes::vector Sha256(bytes::const_span data) {
	return details::Sha<kSha256Size>(SHA256, data);
}

inline void Sha256To(bytes::span dst, bytes::const_span data) {
	details::Sha<kSha256Size>(dst, SHA256, data);
}

template <
	typename ...Args,
	typename = std::enable_if_t<(sizeof...(Args) > 1)>>
[[nodiscard]] inline bytes::vector Sha256(Args &&...args) {
	return details::Sha<kSha256Size>(
		SHA256_CTX(),
		SHA256_Init,
		SHA256_Update,
		SHA256_Final,
		args...);
}

[[nodiscard]] inline bytes::vector Sha512(bytes::const_span data) {
	return details::Sha<kSha512Size>(SHA512, data);
}

inline void Sha512To(bytes::span dst, bytes::const_span data) {
	details::Sha<kSha512Size>(dst, SHA512, data);
}

template <
	typename ...Args,
	typename = std::enable_if_t<(sizeof...(Args) > 1)>>
[[nodiscard]] inline bytes::vector Sha512(Args &&...args) {
	return details::Sha<kSha512Size>(
		SHA512_CTX(),
		SHA512_Init,
		SHA512_Update,
		SHA512_Final,
		args...);
}

inline bytes::vector Pbkdf2Sha512(
		bytes::const_span password,
		bytes::const_span salt,
		int iterations) {
	return details::Pbkdf2<kSha512Size>(
		password,
		salt,
		iterations,
		EVP_sha512());
}

inline bytes::vector HmacSha256(
		bytes::const_span key,
		bytes::const_span data) {
	auto result = bytes::vector(kSha256Size);
	auto length = (unsigned int)kSha256Size;

	HMAC(
		EVP_sha256(),
		key.data(),
		key.size(),
		reinterpret_cast<const unsigned char*>(data.data()),
		data.size(),
		reinterpret_cast<unsigned char*>(result.data()),
		&length);

	return result;
}

} // namespace openssl

#pragma warning(pop)
