// Copyright (c) 2014, German Neuroinformatics Node (G-Node)
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted under the terms of the BSD License. See
// LICENSE file in the root of the Project.

#include <nix/valid/checks.hpp>

#include <functional>
#include <vector>
#include <string>

#include <nix.hpp>

namespace nix {
namespace valid {

bool dimEquals::operator()(const DataArray &array) const {
    return (array.dataExtent().size() == value);
}


bool tagRefsHaveUnits::operator()(const std::vector<DataArray> &references) const {
    bool match = true;
    std::vector<std::string> dims_units;

    for (auto &ref : references) {
        dims_units = getDimensionsUnits(ref);
        if (dims_units.size() != units.size()){
            match = false;
            break;
        }
    }

    return match;
}


bool tagUnitsMatchRefsUnits::operator()(const std::vector<DataArray> &references) const {
    bool match = true;
    std::vector<std::string> dims_units;

    for (auto &ref : references) {
        dims_units = getDimensionsUnits(ref);
        if (!util::isScalable(units, dims_units)) {
            match = false;
            break;
        }
    }

    return match;
}


bool extentsMatchPositions::operator()(const DataArray &positions) const {
    // check that positions.dataExtent()[0] == extents.dataExtent()[0]
    // and that   positions.dataExtent()[1] == extents.dataExtent()[1]
    // and that   positions.dataExtent().size() == extents.dataExtent().size()
    return positions.dataExtent() == boost::any_cast<DataArray>(extents).dataExtent();
}


bool extentsMatchPositions::operator()(const std::vector<double> &positions) const {
    return positions.size() == boost::any_cast<std::vector<double>>(extents).size();
}


bool extentsMatchRefs::operator()(const DataArray &extents) const {
    // we need to check if there are enough coordinates in the positions or extents to point into the references.
    // In case the referenced data is 1-d, positions and extents may be vectors as well. If data is n-d, however,
    // position and extent must be 2-d with the first d the number of points and the second the coordinates for each
    // point. In this case the second dim of the positions/extents must have n entries.
    bool mismatch = false;
    auto extExtent = extents.dataExtent();
    auto it = refs.begin();
    while (!mismatch && (it != refs.end())) {
        auto arrayExtent = (*it).dataExtent();
        mismatch = (arrayExtent.size() > 1 && extExtent.size() == 1) ||
                (extExtent.size() == 2 && extExtent[1] != arrayExtent.size());
        ++it;
    }

    return !mismatch;
}


bool extentsMatchRefs::operator()(const std::vector<double> &extents) const {
    bool mismatch = false;
    auto extSize = extents.size();
    auto it = refs.begin();
    while (!mismatch && (it != refs.end())) {
        auto arrayExtent = (*it).dataExtent();
        mismatch = extSize != arrayExtent.size();
        ++it;
    }

    return !mismatch;
}


bool positionsMatchRefs::operator()(const DataArray &positions) const {
    extentsMatchRefs alias = extentsMatchRefs(refs);

    return alias(positions);
}


bool positionsMatchRefs::operator()(const std::vector<double> &positions) const {
    extentsMatchRefs alias = extentsMatchRefs(refs);

    return alias(positions);
}


bool dimTicksMatchData::operator()(const std::vector<Dimension> &dims) const {
    bool mismatch = false;
    auto it = dims.begin();
    while (!mismatch && it != dims.end()) {
        if ((*it).dimensionType() == DimensionType::Range) {
            ndsize_t dimIndex = (*it).index() - 1;
            if (dimIndex >= data.dataExtent().size()) {
                break;
            }
            auto dim = (*it).asRangeDimension();
            size_t idx = check::fits_in_size_t(dimIndex, "Cannot check ticks: dimension bigger than size_t.");
            mismatch = !(dim.ticks().size() == data.dataExtent()[idx]);
        }
        ++it;
    }
    return !mismatch;
}


bool dimLabelsMatchData::operator()(const std::vector<Dimension> &dims) const {
    bool mismatch = false;
    auto it = dims.begin();
    while (!mismatch && it != dims.end()) {
        if ((*it).dimensionType() == DimensionType::Set) {
            ndsize_t dimIndex = (*it).index() - 1;
            if (dimIndex >= data.dataExtent().size()) {
                break;
            }
            auto dim = (*it).asSetDimension();
            size_t idx = check::fits_in_size_t(dimIndex, "Cannot check labels: dimension bigger than size_t.");
            mismatch = dim.labels().size() > 0 && !(dim.labels().size() == data.dataExtent()[idx]);
        }
        ++it;
    }
    return !mismatch;
}

} // namespace valid
} // namespace nix
