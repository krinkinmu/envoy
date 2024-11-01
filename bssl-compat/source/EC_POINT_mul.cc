#include <openssl/ec.h>
#include <ossl.h>


extern "C" int EC_POINT_mul(const EC_GROUP *group, EC_POINT *r, const BIGNUM *n, const EC_POINT *q, const BIGNUM *m, BN_CTX *ctx) {
  return ossl.ossl_EC_POINT_mul(group, r, n, q, m, ctx);
}
