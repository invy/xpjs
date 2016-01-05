#ifndef XPDATAACCESS_H
#define XPDATAACCESS_H

#include <vector>
#include <string>
#include <cmath>
#include <unordered_map>
#include <XPLMDataAccess.h>
#include <XPLMUtilities.h>



class Data {
public:
    XPLMDataRef ref;
    XPLMDataTypeID typeId;
public:
    Data(const XPLMDataRef &ref, const XPLMDataTypeID& typeId) : ref(ref), typeId(typeId) { }
public:
    int& get(int &data) {
        data = XPLMGetDatai(ref);
        return data;
    }
    float& get(float &data) {
        data = XPLMGetDataf(ref);
        return data;
    }
    double& get(double &data) {
        data = XPLMGetDatad(ref);
        return data;
    }
    template<typename T>
    size_t get(std::vector<T> &data, int (*XPLMAccessor)(XPLMDataRef, T*, int, int)) {
        auto size = XPLMAccessor(ref, nullptr, 0, 0);
        data.resize(size);
        return XPLMAccessor(ref, &(data[0]), 0, size);
    }

    size_t get(std::vector<int> &data) {
        return get(data, XPLMGetDatavi);
    }
    size_t get(std::vector<float> &data) {
        return get(data, XPLMGetDatavf);
    }
    size_t get(std::vector<char> &data) {
        return get<char>(data, Data::_getDatab);
    }
    void set(const int &data) {
        XPLMSetDatai(ref, data);
    }
    int geti_At(const size_t off) {
        int r = 0;
        XPLMGetDatavi(ref, &r, off, 1);
        return r;
    }
    float getf_At(const size_t off) {
        float r = 0;
        XPLMGetDatavf(ref, &r, off, 1);
        return r;
    }


    void set(const float &data) {
        if(!isnan(data))
            XPLMSetDataf(ref, data);
    }
    void set(const double &data) {
        if(!isnan(data))
            XPLMSetDatad(ref, data);
    }
    void set(std::vector<int> &data) {
        XPLMSetDatavi(ref, &(data[0]), 0, data.size());
    }
    void set(std::vector<float> &data) {
        XPLMSetDatavf(ref, &(data[0]), 0, data.size());
    }
    void set(std::vector<char> &data) {
        XPLMSetDatab(ref, &(data[0]), 0, data.size());
    }
    void setAt(int data, const size_t off) {
        XPLMSetDatavi(ref, &data, off, 1);
    }
    void setAt(float data, const size_t off) {
        if(!isnan(data))
            XPLMSetDatavf(ref, &data, off, 1);
    }

private:
    inline static int _getDatab(XPLMDataRef r, char* d, int a, int b) { return XPLMGetDatab(r, (void*)d, a, b); }
};



class XPDataAccess
{
    std::vector<Data> dataRefs;
    std::unordered_map<std::string, size_t> trackingDRefs;
public:
    int requestDataRef(const std::string& drefName) {
        XPLMDebugString(("request dataref: " + drefName + "\n").c_str());
        auto got = trackingDRefs.find(drefName);
        if(got != trackingDRefs.end())
            return got->second;
        XPLMDataRef ref = XPLMFindDataRef(drefName.c_str());
        if(ref == nullptr)
            return -1;
        bool ro = (XPLMCanWriteDataRef(ref) == true);
        auto type = XPLMGetDataRefTypes(ref);
        dataRefs.push_back(Data(ref, type));
        auto refId = dataRefs.size()-1;
        trackingDRefs[drefName] = refId;
        return refId;
    }
    int registerDataRef(const std::string& drefname) {
        // not implemented
        return -1;
    }

    Data& getDataRef(size_t ref) {
        return dataRefs[ref];
    }

    XPDataAccess() { };
};

#endif // XPDATAACCESS_H
