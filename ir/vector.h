/*
Copyright 2013-present Barefoot Networks, Inc. 

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef _IR_VECTOR_H_
#define _IR_VECTOR_H_

#include "dbprint.h"
#include "lib/enumerator.h"
#include "lib/null.h"

namespace IR {


// Specialization of vector which
// - only stores const IR::Node* objects inside (T should derive from Node)
// - inherits from IR::Node itself
class VectorBase : public Node {
 public:
    typedef const Node * const *iterator;
    virtual iterator VectorBase_begin() const = 0;
    virtual iterator VectorBase_end() const = 0;
    virtual size_t size() const = 0;
    virtual bool empty() const = 0;
    iterator begin() const { return VectorBase_begin(); }
    iterator end() const { return VectorBase_end(); }
    VectorBase() = default;
    VectorBase(const VectorBase &) = default;
    VectorBase(VectorBase &&) = default;
    VectorBase &operator=(const VectorBase &) = default;
    VectorBase &operator=(VectorBase &&) = default;
 protected:
    explicit VectorBase(JSONLoader &json) : Node(json) {}
};

// This class should only be used in the IR.
// User-level code should use regular std::vector
template<class T>
class Vector : public VectorBase {
    vector<const T *>   vec;

 public:
    typedef const T* value_type;
    Vector() = default;
    Vector(const Vector &) = default;
    Vector(Vector &&) = default;
    explicit Vector(JSONLoader &json);
    Vector &operator=(const Vector &) = default;
    Vector &operator=(Vector &&) = default;
    explicit Vector(const T *a) {
        vec.emplace_back(std::move(a)); }
    explicit Vector(const vector<const T *> &a) {
        vec.insert(vec.end(), a.begin(), a.end()); }
    Vector(const std::initializer_list<const T *> &a) : vec(a) {}
    static Vector<T>* fromJSON(JSONLoader &json);
    typedef typename vector<const T *>::iterator        iterator;
    typedef typename vector<const T *>::const_iterator  const_iterator;
    iterator begin() { return vec.begin(); }
    const_iterator begin() const { return vec.begin(); }
    VectorBase::iterator VectorBase_begin() const override {
        /* DANGER -- works as long as IR::Node is the first ultimate base class of T */
        return reinterpret_cast<VectorBase::iterator>(&vec[0]); }
    iterator end() { return vec.end(); }
    const_iterator end() const { return vec.end(); }
    VectorBase::iterator VectorBase_end() const override {
        /* DANGER -- works as long as IR::Node is the first ultimate base class of T */
        return reinterpret_cast<VectorBase::iterator>(&vec[0] + vec.size()); }
    std::reverse_iterator<iterator> rbegin() { return vec.rbegin(); }
    std::reverse_iterator<const_iterator> rbegin() const { return vec.rbegin(); }
    std::reverse_iterator<iterator> rend() { return vec.rend(); }
    std::reverse_iterator<const_iterator> rend() const { return vec.rend(); }
    size_t size() const override { return vec.size(); }
    void resize(size_t sz) { vec.resize(sz); }
    bool empty() const override { return vec.empty(); }
    const T* const & front() const { return vec.front(); }
    const T*& front() { return vec.front(); }
    void clear() { vec.clear(); }
    iterator erase(iterator i) { return vec.erase(i); }
    iterator erase(iterator s, iterator e) { return vec.erase(s, e); }
    template<typename ForwardIter>
    iterator insert(iterator i, ForwardIter b, ForwardIter e) {
        /* FIXME -- gcc prior to 4.9 is broken and the insert routine returns void
         * FIXME -- rather than an iterator.  So we recalculate it from an index */
        int index = i - vec.begin();
        vec.insert(i, b, e);
        return vec.begin() + index; }
    iterator append(const Vector<T>& toAppend)
    { return insert(end(), toAppend.begin(), toAppend.end()); }
    iterator insert(iterator i, const T* v) {
        /* FIXME -- gcc prior to 4.9 is broken and the insert routine returns void
         * FIXME -- rather than an iterator.  So we recalculate it from an index */
        int index = i - vec.begin();
        vec.insert(i, v);
        return vec.begin() + index; }
    iterator insert(iterator i, size_t n, const T* v) {
        /* FIXME -- gcc prior to 4.9 is broken and the insert routine returns void
         * FIXME -- rather than an iterator.  So we recalculate it from an index */
        int index = i - vec.begin();
        vec.insert(i, n, v);
        return vec.begin() + index; }

    const T *const &operator[](size_t idx) const { return vec[idx]; }
    const T *&operator[](size_t idx) { return vec[idx]; }
    const T *const &at(size_t idx) const { return vec.at(idx); }
    const T *&at(size_t idx) { return vec.at(idx); }
    template <class... Args> void emplace_back(Args&&... args) {
        vec.emplace_back(new T(std::forward<Args>(args)...)); }
    void push_back(T *a) { vec.push_back(a); }
    void push_back(const T *a) { vec.push_back(a); }
    void pop_back() { vec.pop_back(); }
    const T* const & back() const { return vec.back(); }
    const T*& back() { return vec.back(); }
    template<class U> void push_back(U &a) { vec.push_back(a); }
    void check_null() const { for (auto e : vec) CHECK_NULL(e); }

    IRNODE_SUBCLASS(Vector)
    IRNODE_DECLARE_APPLY_OVERLOAD(Vector)
    bool operator==(const Node &a) const override { return a == *this; }
    // If you get an error about this method not being overridden
    // you are probably using a Vector where you should be using an std::vector.
    bool operator==(const Vector &a) const override { return vec == a.vec; }
    cstring node_type_name() const override {
        return "Vector<" + T::static_type_name() + ">"; }
    static cstring static_type_name() {
        return "Vector<" + T::static_type_name() + ">"; }
    void visit_children(Visitor &v) override;
    void visit_children(Visitor &v) const override;
    virtual void parallel_visit_children(Visitor &v);
    virtual void parallel_visit_children(Visitor &v) const;
    void toJSON(JSONGenerator &json) const override;
    Util::Enumerator<const T*>* getEnumerator() const {
        return Util::Enumerator<const T*>::createEnumerator(vec); }
    template <typename S>
    Util::Enumerator<const S*>* only() const {
        std::function<bool(const T*)> filter = [](const T* d) { return d->template is<S>(); };
        return getEnumerator()->where(filter)->template as<const S*>(); }
};

}  // namespace IR

template<class T, class U> const T *get(const IR::Vector<T> &vec, U name) {
    for (auto el : vec)
        if (el->name == name)
            return el;
    return nullptr; }
template<class T, class U> const T *get(const IR::Vector<T> *vec, U name) {
    if (vec)
        for (auto el : *vec)
            if (el->name == name)
                return el;
    return nullptr; }

#endif /* _IR_VECTOR_H_ */
