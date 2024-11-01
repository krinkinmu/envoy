#include <openssl/obj.h>
#include <ossl.h>


extern "C" int OBJ_cmp(const ASN1_OBJECT *a, const ASN1_OBJECT *b) {
  return ossl.ossl_OBJ_cmp(a, b);
}
