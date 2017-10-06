#ifndef DEMO_SOMA_CLUSTERING_H_
#define DEMO_SOMA_CLUSTERING_H_

// TODO(ahmad): replace with ROOT random number generator
#include <cstdlib>
#include <ctime>
#include <vector>

#include "biodynamo.h"

namespace bdm {

// ----------------------------------------------------------------------------
// This model examplifies the use of extracellur diffusion and shows
// how to extend the default "Cell". In step 0 one can see how an extra
// data member is added and can be accessed throughout the simulation with
// its Get and Set methods. N cells are randomly positioned in space, of which
// half are of type 1 and half of type -1.
//
// Each type secretes a different substance. Cells move towards the gradient of
// their own substance, which results in clusters being formed of cells of the
// same type.
// -----------------------------------------------------------------------------

// 0. Define my custom cell, which extends Cell by adding an extra
// data member cell_type.
BDM_SIM_CLASS(MyCell, Cell) {
  BDM_CLASS_HEADER(MyCellExt, 1, cell_type_);

 public:
  MyCellExt() {}
  // TODO: this needs to be explicitely stated, otherwise empty implementation
  MyCellExt(const std::array<double, 3>& position) : Cell(position) {}

  void SetCellType(int t) { cell_type_[kIdx] = t; }

  int GetCellType() { return cell_type_[kIdx]; }
  // This function is used by ParaView for coloring the cells by their type
  int* GetCellTypePtr() { return cell_type_.data(); }

 private:
  vec<int> cell_type_;
};

// 1a. Define displacement behavior:
// Cells move along the diffusion gradient (from low concentration to high)
struct Chemotaxis {
  template <typename T>
  void Run(T* cell) {
    auto dg_0 = GetDiffusionGrid("Substance_0");
    auto dg_1 = GetDiffusionGrid("Substance_1");

    auto& position = cell->GetPosition();
    std::array<double, 3> gradient_0;
    std::array<double, 3> gradient_1;
    dg_0->GetGradient(position, &gradient_0);
    dg_1->GetGradient(position, &gradient_1);

    std::array<double, 3> diff_gradient;
    // for (int i = 0; i < 3; i++) {
    //   diff_gradient[i] = cell->GetCellType()*(gradient_0[i] -
    //   gradient_1[i])*10;
    // }

    if (cell->GetCellType() == 1) {
      for (int i = 0; i < 3; i++) {
        diff_gradient[i] = gradient_1[i] * 5;
      }
    } else {
      for (int i = 0; i < 3; i++) {
        diff_gradient[i] = gradient_0[i] * 5;
      }
    }

    cell->UpdatePosition(diff_gradient);
    cell->SetPosition(cell->GetMassLocation());
  }

  // Daughter cells inherit this biology module
  bool IsCopied(Event event) const { return true; }
  ClassDefNV(Chemotaxis, 1);
};

// 1b. Define secretion behavior:
struct SubstanceSecretion {
  template <typename T>
  void Run(T* cell) {
    DiffusionGrid* dg = nullptr;
    if (cell->GetCellType() == 1) {
      dg = GetDiffusionGrid("Substance_1");
    } else {
      dg = GetDiffusionGrid("Substance_0");
    }

    auto& secretion_position = cell->GetPosition();
    dg->IncreaseConcentrationBy(secretion_position, 1);
  }

  // Daughter cells inherit this biology module
  bool IsCopied(Event event) const { return true; }
  ClassDefNV(SubstanceSecretion, 1);
};

// 2. Define compile time parameter
template <typename Backend>
struct CompileTimeParam : public DefaultCompileTimeParam<Backend> {
  using BiologyModules = Variant<Chemotaxis, SubstanceSecretion>;
  using AtomicTypes = VariadicTypedef<MyCell>;
};

static double GetNorm(double* currArray) {
  // computes L2 norm of input array
  int c;
  double arraySum = 0;
  for (c = 0; c < 3; c++) {
    arraySum += currArray[c] * currArray[c];
  }
  double res = sqrt(arraySum);

  return res;
}

static double GetL2Distance(double pos1x, double pos1y, double pos1z,
                            double pos2x, double pos2y, double pos2z) {
  // returns distance (L2 norm) between two positions in 3D
  double distArray[3];
  distArray[0] = pos2x - pos1x;
  distArray[1] = pos2y - pos1y;
  distArray[2] = pos2z - pos1z;
  double l2Norm = GetNorm(distArray);
  return l2Norm;
}

// Returns 0 if the cell locations within a subvolume of the total system,
// comprising approximately target_n cells, are arranged as clusters, and 1
// otherwise.
template <typename TResourceManager = ResourceManager<>>
static bool GetCriterion(double spatial_range, int target_n) {
  auto rm = TResourceManager::Get();
  auto my_cells = rm->template Get<MyCell>();

  double* pos_all = my_cells->GetPositionPtr();
  int* types_all = my_cells->GetCellTypePtr();
  int n = my_cells->size();

  int num_close =
      0;  // number of cells that are close (i.e. within a distance of
          // spatial_range)
  double curr_dist;
  int same_type_close = 0;  // number of cells of the same type, and that are
                            // close (i.e. within a distance of spatial_range)
  int diff_type_close = 0;  // number of cells of opposite types, and that are
                            // close (i.e. within a distance of spatial_range)

  vector<vector<double>> pos_sub_vol(n);
  vector<int> types_sub_vol(n);

  double sub_vol_max = n / 8;

  int num_cells_sub_vol = 0;

  // the locations of all cells within the subvolume are copied to array
  // pos_sub_vol
  for (int i1 = 0; i1 < n; i1++) {
    pos_sub_vol[i1] = vector<double>(3);

    if ((fabs(pos_all[3 * i1 + 0] - 0.5) < sub_vol_max) &&
        (fabs(pos_all[3 * i1 + 1] - 0.5) < sub_vol_max) &&
        (fabs(pos_all[3 * i1 + 2] - 0.5) < sub_vol_max)) {
      pos_sub_vol[num_cells_sub_vol][0] = pos_all[3 * i1 + 0];
      pos_sub_vol[num_cells_sub_vol][1] = pos_all[3 * i1 + 1];
      pos_sub_vol[num_cells_sub_vol][2] = pos_all[3 * i1 + 2];
      types_sub_vol[num_cells_sub_vol] = types_all[i1];
      num_cells_sub_vol++;
    }
  }

  printf("number of cells in subvolume: %d\n", num_cells_sub_vol);

  // If there are not enough cells within the subvolume, the correctness
  // criterion is not fulfilled
  if ((((double)(num_cells_sub_vol)) / (double)target_n) < 0.25) {
    std::cout << "not enough cells in subvolume: " << num_cells_sub_vol
              << std::endl;
    return false;
  }

  // If there are too many cells within the subvolume, the correctness
  // criterion is not fulfilled
  if ((((double)(num_cells_sub_vol)) / (double)target_n) > 4) {
    std::cout << "too many cells in subvolume: " << num_cells_sub_vol
              << std::endl;
    return false;
  }

#pragma omp parallel for reduction(+ : same_type_close, diff_type_close, \
                                   num_close)
  for (int i1 = 0; i1 < num_cells_sub_vol; i1++) {
    for (int i2 = i1 + 1; i2 < num_cells_sub_vol; i2++) {
      curr_dist = GetL2Distance(pos_sub_vol[i1][0], pos_sub_vol[i1][1],
                                pos_sub_vol[i1][2], pos_sub_vol[i2][0],
                                pos_sub_vol[i2][1], pos_sub_vol[i2][2]);
      // spatial_range = 5*(250/finalNumCells)^(1/3) = 2.5 with 2000 cells
      if (curr_dist < spatial_range) {
        num_close++;
        if (types_sub_vol[i1] * types_sub_vol[i2] < 0) {
          diff_type_close++;
        } else {
          same_type_close++;
        }
      }
    }
  }

  double correctness_coefficient =
      ((double)diff_type_close) / (num_close + 1.0);

  // check if there are many cells of opposite types located within a close
  // distance, indicative of bad clustering
  if (correctness_coefficient > 0.1) {
    printf("cells in subvolume are not well-clustered: %f\n",
           correctness_coefficient);
    return false;
  }

  // check if clusters are large enough, i.e. whether cells have more than 100
  // cells of the same type located nearby
  double avgNeighbors = ((double)same_type_close / num_cells_sub_vol);
  printf("average neighbors in subvolume: %f\n", avgNeighbors);
  if (avgNeighbors < 5) {
    printf("cells in subvolume do not have enough neighbors: %f\n",
           avgNeighbors);
    return false;
  }

  printf("correctness coefficient: %f\n", correctness_coefficient);

  return true;
}

inline int Simulate(const CommandLineOptions& options) {
  // 3. Define initial model

  // Create an artificial bounds for the simulation space
  Param::bound_space_ = true;
  Param::lbound_ = 0;
  Param::rbound_ = 250;
  int num_cells = 20000;

  auto construct_0 = [](const std::array<double, 3>& position) {
    MyCell cell(position);
    cell.SetDiameter(10);
    cell.SetCellType(1);
    cell.AddBiologyModule(SubstanceSecretion());
    cell.AddBiologyModule(Chemotaxis());
    return cell;
  };
  srand(static_cast<unsigned>(time(0)));
  ModelInitializer::CreateCellsRandom(Param::lbound_, Param::rbound_,
                                      num_cells / 2, construct_0);

  auto construct_1 = [](const std::array<double, 3>& position) {
    MyCell cell(position);
    cell.SetDiameter(10);
    cell.SetCellType(-1);
    cell.AddBiologyModule(SubstanceSecretion());
    cell.AddBiologyModule(Chemotaxis());
    return cell;
  };
  ModelInitializer::CreateCellsRandom(Param::lbound_, Param::rbound_,
                                      num_cells / 2, construct_1);

  // 3. Define the substances that cells may secrete
  // This needs to be done AFTER the cells have been specified
  // Order: substance_name, diffusion_coefficient, decay_constant, resolution
  ModelInitializer::DefineSubstance("Substance_0", 0.5, 0.1, 1);
  ModelInitializer::DefineSubstance("Substance_1", 0.5, 0.1, 1);

  // 4. Run simulation for N timesteps
  Param::use_paraview_ = true;
  Param::write_to_file_ = true;
  Param::write_freq_ = 3000;
  Scheduler<> scheduler;

  scheduler.Simulate(3001);

  auto crit = GetCriterion(5, num_cells / 4);
  if (crit) {
    std::cout << "SUCCESS" << std::endl;
  } else {
    std::cout << "FAILED" << std::endl;
  }
  return !crit;
}

}  // namespace bdm

#endif  // DEMO_SOMA_CLUSTERING_H_
