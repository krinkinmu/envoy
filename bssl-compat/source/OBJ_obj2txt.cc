#include <openssl/obj.h>
#include <ossl.h>


extern "C" int OBJ_obj2txt(char *out, int out_len, const ASN1_OBJECT *obj, int always_return_oid) {
  return ossl.ossl_OBJ_obj2txt(out, out_len, obj, always_return_oid);
}
