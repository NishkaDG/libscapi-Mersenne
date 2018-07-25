//
// Created by meital on 5/30/18.
//

#include "../../include/mid_layer/OpenSSLMac.h"
#include "../../include/primitives/Prg.hpp"



OpenSSLGMAC::OpenSSLGMAC(const shared_ptr<PrgFromOpenSSLAES> & random): random(random){

    EVP_CIPHER_CTX_init(&_ctx);

    EVP_EncryptInit_ex(&_ctx, EVP_aes_128_gcm(), NULL, NULL, NULL); //TODO check return value == 1

    EVP_CIPHER_CTX_ctrl(&_ctx, EVP_CTRL_GCM_SET_IVLEN, 12, NULL);

    iv.resize(12);

}

SecretKey OpenSSLGMAC::generateKey(int keySize) {
    // Generate a random string of bits of length keySize, which has to be greater that zero.

    // If the key size is zero or less - throw exception.
    if (keySize <= 0)
        throw invalid_argument("key size must be greater than 0");

    // The key size has to be a multiple of 8 so that we can obtain an array of random bytes which we use
    // to create the SecretKey.
    if ((keySize % 8) != 0)
        throw invalid_argument("Wrong key size: must be a multiple of 8");

    vector<byte> genBytes(keySize / 8); // Creates a byte vector of size keySize.
    random->getPRGBytes(genBytes, 0, keySize / 8);    // Generates the bytes using the random.
    return SecretKey(genBytes.data(), keySize / 8, "");
}

vector<byte> OpenSSLGMAC::mac(const vector<byte> &msg, int offset, int msgLen) {

    vector<byte> tag(16);


    if (_isIVToSet == true){
        //generate random iv
         // Creates a byte vector of size keySize.
        random->getPRGBytes(iv, 0, 12);    // Generates the bytes using the random.

        EVP_EncryptInit_ex(&_ctx, NULL, NULL, NULL, iv.data());

        _isIVToSet=true;
    }



    //update
    int _unusedOutl;
    EVP_EncryptUpdate(&_ctx, NULL, &_unusedOutl, msg.data(), msgLen); //TODO check return value == 1

    //final
    EVP_EncryptFinal_ex(&_ctx, NULL, &_unusedOutl); //TODO check return value == 1

    EVP_CIPHER_CTX_ctrl(&_ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data());

    //concatenate the iv to the tag, this is part of the final tag

    tag.insert( tag.end(), iv.begin(), iv.end() );


    //initialize the Hmac again in order to enable repeated calls.
    if (0 == (EVP_EncryptInit_ex(&_ctx, NULL, NULL, keyVec.data(), NULL)))
    throw runtime_error("failed to init hmac object");

    return tag;
}

bool OpenSSLGMAC::verify(const vector<byte> &msg, int offset, int msgLength, vector<byte>& tag) {
    if (!isKeySet())
        throw IllegalStateException("secret key isn't set");
    // If the tag size is not the mac size - returns false.
    if ((int) tag.size() != getMacSize())
        return false;
    // Calculate the mac on the msg to get the real tag.

    //get the iv from the tag
    memcpy(&iv[0], &tag[16], 12);



    //set the iv to the one used creating the tag

    EVP_EncryptInit_ex(&_ctx, NULL, NULL, NULL, iv.data());

    _isIVToSet=false;//do not set the iv, use the current iv for this mac


    vector<byte> macTag = mac(msg, offset, msgLength);


    // Compares the real tag to the given tag.
    // for code-security reasons, the comparison is fully performed. that is, even if we know already after the first few bits
    // that the tag is not equal to the mac, we continue the checking until the end of the tag bits.
    bool equal = true;
    int length = macTag.size();
    for (int i = 0; i<length; i++) {
        if (macTag[i] != tag[i]) {
            equal = false;
        }
    }
    return equal;
}



void OpenSSLGMAC::setMacKey(SecretKey & secretKey) {
    // Initialize the Hmac object with the given key.
    keyVec = secretKey.getEncoded();
    //set the key, the iv will be set again after finalizing
    EVP_EncryptInit_ex(&_ctx, NULL, NULL, keyVec.data(), NULL);
    _isKeySet = true;
}


void OpenSSLGMAC::update(vector<byte> & msg, int offset, int msgLen) {
    if (!isKeySet())
        throw IllegalStateException("secret key isn't set");

    //update
    int _unusedOutl;
    if (0 == (EVP_EncryptUpdate(&_ctx, NULL, &_unusedOutl, &msg[offset], msgLen)))
        throw runtime_error("failed to update gmac object");

    _isIVToSet=false;

}


void OpenSSLGMAC::doFinal(vector<byte> & msg, int offset, int msgLength, vector<byte> & tag_res) {

    tag_res = mac(msg, offset, msgLength);

}


OpenSSLGMAC::~OpenSSLGMAC(){

    EVP_CIPHER_CTX_cleanup(&_ctx);
}