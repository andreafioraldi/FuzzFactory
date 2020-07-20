#include "fuzzfactory.hpp"

using namespace fuzzfactory;

static size_t TypeSizeToSizeIndex(uint32_t TypeSize) {
  size_t Res = countTrailingZeros(TypeSize / 8);
  return Res;
}

class BufFeedback : public DomainFeedback<BufFeedback> {
public:
    BufFeedback(Module& M) : DomainFeedback<BufFeedback>(M, "__afl_buf_start_dsf") {
    
      int LongSize = M.getDataLayout().getPointerSizeInBits();
      
      IntTypeSized[0] = getIntTy(8);
	    IntTypeSized[1] = getIntTy(16);
	    IntTypeSized[2] = getIntTy(32);
	    IntTypeSized[3] = getIntTy(64);
      SizeType = IntTypeSized[TypeSizeToSizeIndex(LongSize)];

      HandleMallocFn = resolveFunction("__afl_buf_handle_malloc", VoidTy, {Int32Ty, getIntPtrTy(8), SizeType});
      HandleCallocFn = resolveFunction("__afl_buf_handle_calloc", VoidTy, {Int32Ty, getIntPtrTy(8), SizeType, SizeType});
      HandleFreeFn = resolveFunction("__afl_buf_handle_free", VoidTy, {getIntPtrTy(8)});
      
      AccessFn = resolveFunction("__afl_buf_access", VoidTy, {getIntPtrTy(8)});
    
    }

    void visitCallInst(CallInst& call) {
        Function* callee = call.getCalledFunction();
        if (!callee) { return; } // No callee for indirect calls

        if (callee->getName() == "malloc") { // Handle malloc
            auto key = createProgramLocation(); // static random value
            auto irb = insert_after(call); // Get a handle to the LLVM IR Builder at this point
            auto bytes = irb.CreateBitCast(call.getArgOperand(0), SizeType);
            auto ptr = irb.CreatePointerCast(&call, getIntPtrTy(8));

            irb.CreateCall(HandleMallocFn, {key, ptr, bytes}); 
        } else if (callee->getName() == "calloc") { // Handle calloc
            auto key = createProgramLocation(); // static random value
            auto irb = insert_after(call); // Get a handle to the LLVM IR Builder at this point
            auto len = irb.CreateBitCast(call.getArgOperand(0), SizeType);
            auto cnt = irb.CreateBitCast(call.getArgOperand(1), SizeType);
            auto ptr = irb.CreatePointerCast(&call, getIntPtrTy(8));

            irb.CreateCall(HandleCallocFn, {key, ptr, len, cnt}); 
        } else if (callee->getName() == "free") { // Handle free
            auto irb = insert_after(call); // Get a handle to the LLVM IR Builder at this point
            auto ptr = irb.CreatePointerCast(call.getArgOperand(0), getIntPtrTy(8));

            irb.CreateCall(HandleFreeFn, {ptr}); 
        }
    }
    
    void visitStoreInst(StoreInst& inst) {
      auto irb = insert_after(inst);
      auto ptr = irb.CreatePointerCast(inst.getPointerOperand(), getIntPtrTy(8));

      irb.CreateCall(AccessFn, {ptr}); 
    }
    
    void visitLoadInst(LoadInst& inst) {
      auto irb = insert_after(inst);
      auto ptr = irb.CreatePointerCast(inst.getPointerOperand(), getIntPtrTy(8));

      irb.CreateCall(AccessFn, {ptr}); 
    }

private:
    Type *IntTypeSized[4];
    Type *SizeType;
    Function* HandleMallocFn, *HandleCallocFn, *HandleReallocFn, *HandleFreeFn, *AccessFn;
};

FUZZFACTORY_REGISTER_DOMAIN(BufFeedback);
