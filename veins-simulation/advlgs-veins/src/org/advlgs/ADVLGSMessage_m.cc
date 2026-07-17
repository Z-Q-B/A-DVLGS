//
// Generated file, do not edit! Created by opp_msgtool 6.1 from src/org/advlgs/ADVLGSMessage.msg.
//

// Disable warnings about unused variables, empty switch stmts, etc:
#ifdef _MSC_VER
#  pragma warning(disable:4101)
#  pragma warning(disable:4065)
#endif

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wshadow"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wunused-parameter"
#  pragma clang diagnostic ignored "-Wc++98-compat"
#  pragma clang diagnostic ignored "-Wunreachable-code-break"
#  pragma clang diagnostic ignored "-Wold-style-cast"
#elif defined(__GNUC__)
#  pragma GCC diagnostic ignored "-Wshadow"
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
#  pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

#include <iostream>
#include <sstream>
#include <memory>
#include <type_traits>
#include "ADVLGSMessage_m.h"

namespace omnetpp {

// Template pack/unpack rules. They are declared *after* a1l type-specific pack functions for multiple reasons.
// They are in the omnetpp namespace, to allow them to be found by argument-dependent lookup via the cCommBuffer argument

// Packing/unpacking an std::vector
template<typename T, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::vector<T,A>& v)
{
    int n = v.size();
    doParsimPacking(buffer, n);
    for (int i = 0; i < n; i++)
        doParsimPacking(buffer, v[i]);
}

template<typename T, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::vector<T,A>& v)
{
    int n;
    doParsimUnpacking(buffer, n);
    v.resize(n);
    for (int i = 0; i < n; i++)
        doParsimUnpacking(buffer, v[i]);
}

// Packing/unpacking an std::list
template<typename T, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::list<T,A>& l)
{
    doParsimPacking(buffer, (int)l.size());
    for (typename std::list<T,A>::const_iterator it = l.begin(); it != l.end(); ++it)
        doParsimPacking(buffer, (T&)*it);
}

template<typename T, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::list<T,A>& l)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        l.push_back(T());
        doParsimUnpacking(buffer, l.back());
    }
}

// Packing/unpacking an std::set
template<typename T, typename Tr, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::set<T,Tr,A>& s)
{
    doParsimPacking(buffer, (int)s.size());
    for (typename std::set<T,Tr,A>::const_iterator it = s.begin(); it != s.end(); ++it)
        doParsimPacking(buffer, *it);
}

template<typename T, typename Tr, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::set<T,Tr,A>& s)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        T x;
        doParsimUnpacking(buffer, x);
        s.insert(x);
    }
}

// Packing/unpacking an std::map
template<typename K, typename V, typename Tr, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::map<K,V,Tr,A>& m)
{
    doParsimPacking(buffer, (int)m.size());
    for (typename std::map<K,V,Tr,A>::const_iterator it = m.begin(); it != m.end(); ++it) {
        doParsimPacking(buffer, it->first);
        doParsimPacking(buffer, it->second);
    }
}

template<typename K, typename V, typename Tr, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::map<K,V,Tr,A>& m)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        K k; V v;
        doParsimUnpacking(buffer, k);
        doParsimUnpacking(buffer, v);
        m[k] = v;
    }
}

// Default pack/unpack function for arrays
template<typename T>
void doParsimArrayPacking(omnetpp::cCommBuffer *b, const T *t, int n)
{
    for (int i = 0; i < n; i++)
        doParsimPacking(b, t[i]);
}

template<typename T>
void doParsimArrayUnpacking(omnetpp::cCommBuffer *b, T *t, int n)
{
    for (int i = 0; i < n; i++)
        doParsimUnpacking(b, t[i]);
}

// Default rule to prevent compiler from choosing base class' doParsimPacking() function
template<typename T>
void doParsimPacking(omnetpp::cCommBuffer *, const T& t)
{
    throw omnetpp::cRuntimeError("Parsim error: No doParsimPacking() function for type %s", omnetpp::opp_typename(typeid(t)));
}

template<typename T>
void doParsimUnpacking(omnetpp::cCommBuffer *, T& t)
{
    throw omnetpp::cRuntimeError("Parsim error: No doParsimUnpacking() function for type %s", omnetpp::opp_typename(typeid(t)));
}

}  // namespace omnetpp

namespace advlgs {

Register_Class(ADVLGSMessage)

ADVLGSMessage::ADVLGSMessage(const char *name, short kind) : ::veins::BaseFrame1609_4(name, kind)
{
}

ADVLGSMessage::ADVLGSMessage(const ADVLGSMessage& other) : ::veins::BaseFrame1609_4(other)
{
    copy(other);
}

ADVLGSMessage::~ADVLGSMessage()
{
}

ADVLGSMessage& ADVLGSMessage::operator=(const ADVLGSMessage& other)
{
    if (this == &other) return *this;
    ::veins::BaseFrame1609_4::operator=(other);
    copy(other);
    return *this;
}

void ADVLGSMessage::copy(const ADVLGSMessage& other)
{
    this->demoData = other.demoData;
    this->senderAddress = other.senderAddress;
    this->serial = other.serial;
    this->generatedAt = other.generatedAt;
    this->signedAt = other.signedAt;
    this->sentAt = other.sentAt;
    this->signDurationSeconds = other.signDurationSeconds;
    this->serviceDomain = other.serviceDomain;
    this->signature = other.signature;
    this->linkTag = other.linkTag;
}

void ADVLGSMessage::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::veins::BaseFrame1609_4::parsimPack(b);
    doParsimPacking(b,this->demoData);
    doParsimPacking(b,this->senderAddress);
    doParsimPacking(b,this->serial);
    doParsimPacking(b,this->generatedAt);
    doParsimPacking(b,this->signedAt);
    doParsimPacking(b,this->sentAt);
    doParsimPacking(b,this->signDurationSeconds);
    doParsimPacking(b,this->serviceDomain);
    doParsimPacking(b,this->signature);
    doParsimPacking(b,this->linkTag);
}

void ADVLGSMessage::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::veins::BaseFrame1609_4::parsimUnpack(b);
    doParsimUnpacking(b,this->demoData);
    doParsimUnpacking(b,this->senderAddress);
    doParsimUnpacking(b,this->serial);
    doParsimUnpacking(b,this->generatedAt);
    doParsimUnpacking(b,this->signedAt);
    doParsimUnpacking(b,this->sentAt);
    doParsimUnpacking(b,this->signDurationSeconds);
    doParsimUnpacking(b,this->serviceDomain);
    doParsimUnpacking(b,this->signature);
    doParsimUnpacking(b,this->linkTag);
}

const char * ADVLGSMessage::getDemoData() const
{
    return this->demoData.c_str();
}

void ADVLGSMessage::setDemoData(const char * demoData)
{
    this->demoData = demoData;
}

const ::veins::LAddress::L2Type& ADVLGSMessage::getSenderAddress() const
{
    return this->senderAddress;
}

void ADVLGSMessage::setSenderAddress(const ::veins::LAddress::L2Type& senderAddress)
{
    this->senderAddress = senderAddress;
}

int ADVLGSMessage::getSerial() const
{
    return this->serial;
}

void ADVLGSMessage::setSerial(int serial)
{
    this->serial = serial;
}

::omnetpp::simtime_t ADVLGSMessage::getGeneratedAt() const
{
    return this->generatedAt;
}

void ADVLGSMessage::setGeneratedAt(::omnetpp::simtime_t generatedAt)
{
    this->generatedAt = generatedAt;
}

::omnetpp::simtime_t ADVLGSMessage::getSignedAt() const
{
    return this->signedAt;
}

void ADVLGSMessage::setSignedAt(::omnetpp::simtime_t signedAt)
{
    this->signedAt = signedAt;
}

::omnetpp::simtime_t ADVLGSMessage::getSentAt() const
{
    return this->sentAt;
}

void ADVLGSMessage::setSentAt(::omnetpp::simtime_t sentAt)
{
    this->sentAt = sentAt;
}

double ADVLGSMessage::getSignDurationSeconds() const
{
    return this->signDurationSeconds;
}

void ADVLGSMessage::setSignDurationSeconds(double signDurationSeconds)
{
    this->signDurationSeconds = signDurationSeconds;
}

const char * ADVLGSMessage::getServiceDomain() const
{
    return this->serviceDomain.c_str();
}

void ADVLGSMessage::setServiceDomain(const char * serviceDomain)
{
    this->serviceDomain = serviceDomain;
}

const char * ADVLGSMessage::getSignature() const
{
    return this->signature.c_str();
}

void ADVLGSMessage::setSignature(const char * signature)
{
    this->signature = signature;
}

const char * ADVLGSMessage::getLinkTag() const
{
    return this->linkTag.c_str();
}

void ADVLGSMessage::setLinkTag(const char * linkTag)
{
    this->linkTag = linkTag;
}

class ADVLGSMessageDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertyNames;
    enum FieldConstants {
        FIELD_demoData,
        FIELD_senderAddress,
        FIELD_serial,
        FIELD_generatedAt,
        FIELD_signedAt,
        FIELD_sentAt,
        FIELD_signDurationSeconds,
        FIELD_serviceDomain,
        FIELD_signature,
        FIELD_linkTag,
    };
  public:
    ADVLGSMessageDescriptor();
    virtual ~ADVLGSMessageDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyName) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyName) const override;
    virtual int getFieldArraySize(omnetpp::any_ptr object, int field) const override;
    virtual void setFieldArraySize(omnetpp::any_ptr object, int field, int size) const override;

    virtual const char *getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const override;
    virtual std::string getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const override;
    virtual omnetpp::cValue getFieldValue(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual omnetpp::any_ptr getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const override;
};

Register_ClassDescriptor(ADVLGSMessageDescriptor)

ADVLGSMessageDescriptor::ADVLGSMessageDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(advlgs::ADVLGSMessage)), "veins::BaseFrame1609_4")
{
    propertyNames = nullptr;
}

ADVLGSMessageDescriptor::~ADVLGSMessageDescriptor()
{
    delete[] propertyNames;
}

bool ADVLGSMessageDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<ADVLGSMessage *>(obj)!=nullptr;
}

const char **ADVLGSMessageDescriptor::getPropertyNames() const
{
    if (!propertyNames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
        const char **baseNames = base ? base->getPropertyNames() : nullptr;
        propertyNames = mergeLists(baseNames, names);
    }
    return propertyNames;
}

const char *ADVLGSMessageDescriptor::getProperty(const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? base->getProperty(propertyName) : nullptr;
}

int ADVLGSMessageDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? 10+base->getFieldCount() : 10;
}

unsigned int ADVLGSMessageDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeFlags(field);
        field -= base->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISEDITABLE,    // FIELD_demoData
        0,    // FIELD_senderAddress
        FD_ISEDITABLE,    // FIELD_serial
        FD_ISEDITABLE,    // FIELD_generatedAt
        FD_ISEDITABLE,    // FIELD_signedAt
        FD_ISEDITABLE,    // FIELD_sentAt
        FD_ISEDITABLE,    // FIELD_signDurationSeconds
        FD_ISEDITABLE,    // FIELD_serviceDomain
        FD_ISEDITABLE,    // FIELD_signature
        FD_ISEDITABLE,    // FIELD_linkTag
    };
    return (field >= 0 && field < 10) ? fieldTypeFlags[field] : 0;
}

const char *ADVLGSMessageDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldName(field);
        field -= base->getFieldCount();
    }
    static const char *fieldNames[] = {
        "demoData",
        "senderAddress",
        "serial",
        "generatedAt",
        "signedAt",
        "sentAt",
        "signDurationSeconds",
        "serviceDomain",
        "signature",
        "linkTag",
    };
    return (field >= 0 && field < 10) ? fieldNames[field] : nullptr;
}

int ADVLGSMessageDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    int baseIndex = base ? base->getFieldCount() : 0;
    if (strcmp(fieldName, "demoData") == 0) return baseIndex + 0;
    if (strcmp(fieldName, "senderAddress") == 0) return baseIndex + 1;
    if (strcmp(fieldName, "serial") == 0) return baseIndex + 2;
    if (strcmp(fieldName, "generatedAt") == 0) return baseIndex + 3;
    if (strcmp(fieldName, "signedAt") == 0) return baseIndex + 4;
    if (strcmp(fieldName, "sentAt") == 0) return baseIndex + 5;
    if (strcmp(fieldName, "signDurationSeconds") == 0) return baseIndex + 6;
    if (strcmp(fieldName, "serviceDomain") == 0) return baseIndex + 7;
    if (strcmp(fieldName, "signature") == 0) return baseIndex + 8;
    if (strcmp(fieldName, "linkTag") == 0) return baseIndex + 9;
    return base ? base->findField(fieldName) : -1;
}

const char *ADVLGSMessageDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeString(field);
        field -= base->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "string",    // FIELD_demoData
        "veins::LAddress::L2Type",    // FIELD_senderAddress
        "int",    // FIELD_serial
        "omnetpp::simtime_t",    // FIELD_generatedAt
        "omnetpp::simtime_t",    // FIELD_signedAt
        "omnetpp::simtime_t",    // FIELD_sentAt
        "double",    // FIELD_signDurationSeconds
        "string",    // FIELD_serviceDomain
        "string",    // FIELD_signature
        "string",    // FIELD_linkTag
    };
    return (field >= 0 && field < 10) ? fieldTypeStrings[field] : nullptr;
}

const char **ADVLGSMessageDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldPropertyNames(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

const char *ADVLGSMessageDescriptor::getFieldProperty(int field, const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldProperty(field, propertyName);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

int ADVLGSMessageDescriptor::getFieldArraySize(omnetpp::any_ptr object, int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldArraySize(object, field);
        field -= base->getFieldCount();
    }
    ADVLGSMessage *pp = omnetpp::fromAnyPtr<ADVLGSMessage>(object); (void)pp;
    switch (field) {
        default: return 0;
    }
}

void ADVLGSMessageDescriptor::setFieldArraySize(omnetpp::any_ptr object, int field, int size) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldArraySize(object, field, size);
            return;
        }
        field -= base->getFieldCount();
    }
    ADVLGSMessage *pp = omnetpp::fromAnyPtr<ADVLGSMessage>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set array size of field %d of class 'ADVLGSMessage'", field);
    }
}

const char *ADVLGSMessageDescriptor::getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldDynamicTypeString(object,field,i);
        field -= base->getFieldCount();
    }
    ADVLGSMessage *pp = omnetpp::fromAnyPtr<ADVLGSMessage>(object); (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string ADVLGSMessageDescriptor::getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValueAsString(object,field,i);
        field -= base->getFieldCount();
    }
    ADVLGSMessage *pp = omnetpp::fromAnyPtr<ADVLGSMessage>(object); (void)pp;
    switch (field) {
        case FIELD_demoData: return oppstring2string(pp->getDemoData());
        case FIELD_senderAddress: return "";
        case FIELD_serial: return long2string(pp->getSerial());
        case FIELD_generatedAt: return simtime2string(pp->getGeneratedAt());
        case FIELD_signedAt: return simtime2string(pp->getSignedAt());
        case FIELD_sentAt: return simtime2string(pp->getSentAt());
        case FIELD_signDurationSeconds: return double2string(pp->getSignDurationSeconds());
        case FIELD_serviceDomain: return oppstring2string(pp->getServiceDomain());
        case FIELD_signature: return oppstring2string(pp->getSignature());
        case FIELD_linkTag: return oppstring2string(pp->getLinkTag());
        default: return "";
    }
}

void ADVLGSMessageDescriptor::setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValueAsString(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    ADVLGSMessage *pp = omnetpp::fromAnyPtr<ADVLGSMessage>(object); (void)pp;
    switch (field) {
        case FIELD_demoData: pp->setDemoData((value)); break;
        case FIELD_serial: pp->setSerial(string2long(value)); break;
        case FIELD_generatedAt: pp->setGeneratedAt(string2simtime(value)); break;
        case FIELD_signedAt: pp->setSignedAt(string2simtime(value)); break;
        case FIELD_sentAt: pp->setSentAt(string2simtime(value)); break;
        case FIELD_signDurationSeconds: pp->setSignDurationSeconds(string2double(value)); break;
        case FIELD_serviceDomain: pp->setServiceDomain((value)); break;
        case FIELD_signature: pp->setSignature((value)); break;
        case FIELD_linkTag: pp->setLinkTag((value)); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'ADVLGSMessage'", field);
    }
}

omnetpp::cValue ADVLGSMessageDescriptor::getFieldValue(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValue(object,field,i);
        field -= base->getFieldCount();
    }
    ADVLGSMessage *pp = omnetpp::fromAnyPtr<ADVLGSMessage>(object); (void)pp;
    switch (field) {
        case FIELD_demoData: return pp->getDemoData();
        case FIELD_senderAddress: return omnetpp::toAnyPtr(&pp->getSenderAddress()); break;
        case FIELD_serial: return pp->getSerial();
        case FIELD_generatedAt: return pp->getGeneratedAt().dbl();
        case FIELD_signedAt: return pp->getSignedAt().dbl();
        case FIELD_sentAt: return pp->getSentAt().dbl();
        case FIELD_signDurationSeconds: return pp->getSignDurationSeconds();
        case FIELD_serviceDomain: return pp->getServiceDomain();
        case FIELD_signature: return pp->getSignature();
        case FIELD_linkTag: return pp->getLinkTag();
        default: throw omnetpp::cRuntimeError("Cannot return field %d of class 'ADVLGSMessage' as cValue -- field index out of range?", field);
    }
}

void ADVLGSMessageDescriptor::setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValue(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    ADVLGSMessage *pp = omnetpp::fromAnyPtr<ADVLGSMessage>(object); (void)pp;
    switch (field) {
        case FIELD_demoData: pp->setDemoData(value.stringValue()); break;
        case FIELD_serial: pp->setSerial(omnetpp::checked_int_cast<int>(value.intValue())); break;
        case FIELD_generatedAt: pp->setGeneratedAt(value.doubleValue()); break;
        case FIELD_signedAt: pp->setSignedAt(value.doubleValue()); break;
        case FIELD_sentAt: pp->setSentAt(value.doubleValue()); break;
        case FIELD_signDurationSeconds: pp->setSignDurationSeconds(value.doubleValue()); break;
        case FIELD_serviceDomain: pp->setServiceDomain(value.stringValue()); break;
        case FIELD_signature: pp->setSignature(value.stringValue()); break;
        case FIELD_linkTag: pp->setLinkTag(value.stringValue()); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'ADVLGSMessage'", field);
    }
}

const char *ADVLGSMessageDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructName(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    };
}

omnetpp::any_ptr ADVLGSMessageDescriptor::getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructValuePointer(object, field, i);
        field -= base->getFieldCount();
    }
    ADVLGSMessage *pp = omnetpp::fromAnyPtr<ADVLGSMessage>(object); (void)pp;
    switch (field) {
        case FIELD_senderAddress: return omnetpp::toAnyPtr(&pp->getSenderAddress()); break;
        default: return omnetpp::any_ptr(nullptr);
    }
}

void ADVLGSMessageDescriptor::setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldStructValuePointer(object, field, i, ptr);
            return;
        }
        field -= base->getFieldCount();
    }
    ADVLGSMessage *pp = omnetpp::fromAnyPtr<ADVLGSMessage>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'ADVLGSMessage'", field);
    }
}

}  // namespace advlgs

namespace omnetpp {

}  // namespace omnetpp

