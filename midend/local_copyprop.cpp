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

#include "local_copyprop.h"
#include "has_side_effects.h"
#include "expr_uses.h"

class P4::DoLocalCopyPropagation::ElimDead : public Transform {
    DoLocalCopyPropagation &self;
    const IR::Node *preorder(IR::Declaration_Variable *var) override {
        if (auto local = ::getref(self.available, var->name)) {
            BUG_CHECK(local->local, "Non-local local var?");
            if (!local->live) {
                LOG3("  removing dead local " << var->name);
                return nullptr; }
        } else {
            BUG("local %s isn't in available table", var->name); }
        return var; }
    IR::AssignmentStatement *postorder(IR::AssignmentStatement *as) override {
        if (auto dest = as->left->to<IR::PathExpression>()) {
            if (auto var = ::getref(self.available, dest->path->name)) {
                if (var->local && !var->live) {
                    LOG3("  removing dead assignment to " << dest->path->name);
                    return nullptr; } } }
        return as; }

 public:
    explicit ElimDead(DoLocalCopyPropagation &self) : self(self) {}
};

void P4::DoLocalCopyPropagation::flow_merge(Visitor &a_) {
    auto &a = dynamic_cast<DoLocalCopyPropagation &>(a_);
    BUG_CHECK(in_action == a.in_action, "inconsitent DoLocalCopyPropagation state on merge");
    for (auto &var : available) {
        if (auto merge = ::getref(a.available, var.first)) {
            if (merge->val != var.second.val)
                var.second.val = nullptr;
            if (merge->live)
                var.second.live = true;
        } else {
            var.second.val = nullptr; } }
}

void P4::DoLocalCopyPropagation::dropValuesUsing(cstring name) {
    for (auto &var : available) {
        if (var.first == name) {
            LOG4("   dropping " << name << " as it is being assigned to");
            var.second.val = nullptr;
        } else if (var.second.val && exprUses(var.second.val, name)) {
            LOG4("   dropping " << var.first << " as it is use" << name);
            var.second.val = nullptr; } }
}

const IR::Node *P4::DoLocalCopyPropagation::postorder(IR::Declaration_Variable *var) {
    LOG1("Visiting " << getOriginal());
    if (!in_action) return var;
    if (available.count(var->name))
        BUG("duplicate var declaration for %s", var->name);
    auto &local = available[var->name];
    local.local = true;
    if (var->initializer) {
        if (!hasSideEffects(var->initializer)) {
            LOG3("  saving init value for " << var->name);
            local.val = var->initializer;
        } else {
            local.live = true; } }
    return var;
}

const IR::Expression *P4::DoLocalCopyPropagation::postorder(IR::PathExpression *path) {
    if (auto var = ::getref(available, path->path->name)) {
        if (isWrite()) {
            return path;
        } else if (var->val) {
            LOG3("  propagating value for " << path->path->name);
            return var->val;
        } else {
            LOG4("  using " << path->path->name << " with no propagated value");
            var->live = true; } }
    return path;
}

IR::AssignmentStatement *P4::DoLocalCopyPropagation::postorder(IR::AssignmentStatement *as) {
    if (as->left == as->right) {   // FIXME -- need deep equals here?
        LOG3("  removing noop assignment " << *as);
        return nullptr; }
    if (!in_action) return as;
    if (auto dest = as->left->to<IR::PathExpression>()) {
        dropValuesUsing(dest->path->name);
        if (!hasSideEffects(as->right)) {
            if (as->right->is<IR::ListExpression>()) {
                /* FIXME -- List Expressions need to be turned into constructor calls before
                 * we can copyprop them */
                 return as; }
            LOG3("  saving value for " << dest->path->name);
            available[dest->path->name].val = as->right; } }
    return as;
}

IR::MethodCallExpression *P4::DoLocalCopyPropagation::postorder(IR::MethodCallExpression *mc) {
    if (!in_action) return mc;
    auto type = mc->method->type->to<IR::Type_Method>();
    CHECK_NULL(type);
    int idx = 0;
    for (auto param : *type->parameters->parameters) {
        if (param->direction == IR::Direction::Out || param->direction == IR::Direction::InOut) {
            if (auto arg = mc->arguments->at(idx)->to<IR::PathExpression>()) {
                dropValuesUsing(arg->path->name); } }
        ++idx; }
    return mc;
}

IR::Primitive *P4::DoLocalCopyPropagation::postorder(IR::Primitive *prim) {
    if (!in_action) return prim;
    for (unsigned idx = 0; idx < prim->operands.size(); ++idx) {
        if (prim->isOutput(idx)) {
            dropValuesUsing(prim->operands.at(idx)->toString()); } }
    return prim;
}

IR::P4Action *P4::DoLocalCopyPropagation::preorder(IR::P4Action *act) {
    in_action = true;
    if (!available.empty()) BUG("corrupt internal data struct");
    LOG2("DoLocalCopyPropagation working on action " << act->name);
    LOG4(act);
    return act;
}

IR::P4Action *P4::DoLocalCopyPropagation::postorder(IR::P4Action *act) {
    LOG5("DoLocalCopyPropagation before ElimDead " << act->name);
    LOG5(act);
    act->body = act->body->apply(ElimDead(*this))->to<IR::BlockStatement>();
    in_action = false;
    available.clear();
    LOG3("DoLocalCopyPropagation finished action " << act->name);
    LOG4(act);
    return act;
}
