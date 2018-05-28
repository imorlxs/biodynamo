#include "unit/dividing_cell_op_test.h"
#include "unit/default_ctparam.h"

namespace bdm {
namespace dividing_cell_op_test_internal {

// ComputeAos test is in different binary, because it requires different compile
// time parameter (separate_binary/dividing_cell_op_aos_test.cc)

TEST(DividingCellOpTest, ComputeSoa) { RunTest<Cell>(); }

}  // namespace dividing_cell_op_test_internal
}  // namespace bdm