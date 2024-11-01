#include <openssl/asn1.h>
#include <ossl.h>


extern "C" BIGNUM *ASN1_ENUMERATED_to_BN(const ASN1_ENUMERATED *ai, BIGNUM *bn) {
  return ossl.ossl_ASN1_ENUMERATED_to_BN(ai, bn);
}
