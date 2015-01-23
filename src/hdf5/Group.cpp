// Copyright (c) 2013, German Neuroinformatics Node (G-Node)
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted under the terms of the BSD License. See
// LICENSE file in the root of the Project.

#include <nix/hdf5/Group.hpp>
#include <nix/hdf5/ExceptionHDF5.hpp>
#include <nix/util/util.hpp>
#include <boost/multi_array.hpp>
#include <H5Gpublic.h>


namespace nix {
namespace hdf5 {

optGroup::optGroup(const Group &parent, const std::string &g_name)
    : parent(parent), g_name(g_name)
{}

boost::optional<Group> optGroup::operator() (bool create) const {
    if (parent.hasGroup(g_name)) {
        g = boost::optional<Group>(parent.openGroup(g_name));
    } else if (create) {
        g = boost::optional<Group>(parent.openGroup(g_name, true));
    }
    return g;
}


Group::Group() : BaseHDF5() {}


Group::Group(hid_t hid) : BaseHDF5(hid) {}


Group::Group(const Group &other) : BaseHDF5(other) {}


Group::Group(const H5::Group &h5group) : BaseHDF5(h5group.getLocId()) {}


bool Group::hasObject(const std::string &name) const {
    // empty string should return false, not exception (which H5Lexists would)
    if (name.empty()) {
        return false;
    }
    htri_t res = H5Lexists(hid, name.c_str(), H5P_DEFAULT);
    return res;
}

bool Group::objectOfType(const std::string &name, H5O_type_t type) const {
    herr_t err;
    H5O_info_t info;

    hid_t obj = H5Oopen(hid, name.c_str(), H5P_DEFAULT);

    if (!H5Iis_valid(obj)) {
        return false;
    }

    err = H5Oget_info(obj, &info);

    H5Error::check(err, "Could not obtain object info");

    bool res = info.type == type;

    H5Oclose(obj);
    return res;
}

size_t Group::objectCount() const {
    hsize_t n_objs;
    herr_t res = H5Gget_num_objs(hid, &n_objs);
    H5Error::check(res, "Could not get object count");
    return n_objs;
}


boost::optional<Group> Group::findGroupByAttribute(const std::string &attribute, const std::string &value) const {
    boost::optional<Group> ret;

    // look up first direct sub-group that has given attribute with given value
    for (size_t index = 0; index < objectCount(); index++) {
        std::string obj_name = objectName(index);
        if(hasGroup(obj_name)) {
            Group group = openGroup(obj_name, false);
            if(group.hasAttr(attribute)) {
                std::string attr_value;
                group.getAttr(attribute, attr_value);
                if (attr_value == value) {
                    ret = group;
                    break;
                }
            }
        }
    }

    return ret;
}


boost::optional<DataSet> Group::findDataByAttribute(const std::string &attribute, const std::string &value) const {
    std::vector<DataSet> dsets;
    boost::optional<DataSet> ret;

    // look up all direct sub-datasets that have the given attribute
    for (size_t index = 0; index < objectCount(); index++) {
        std::string obj_name = objectName(index);
        if(hasData(obj_name)) {
            hid_t did = H5Dopen(hid, obj_name.c_str(), H5P_DEFAULT);

            if (H5Aexists(did, attribute.c_str())) {
                dsets.emplace_back(H5::DataSet(did));
            } else {
                H5Dclose(did);
            }
        }
    }
    // look for first dataset with given attribute set to given value
    auto found = std::find_if(dsets.begin(), dsets.end(),
                 [value, attribute](DataSet &dset) {
                     std::string attr_value;
                     dset.getAttr(attribute, attr_value);
                     return attr_value == value; });
    if(found != dsets.end()) ret = *found;

    return ret;
}


std::string Group::objectName(size_t index) const {
    // check if index valid
    if(index > objectCount()) {
        throw OutOfBounds("No object at given index", index);
    }

    std::string str_name;
    // check whether name is found by index
    ssize_t name_len = H5Lget_name_by_idx(hid,
                                                  ".",
                                                  H5_INDEX_NAME,
                                                  H5_ITER_NATIVE,
                                                  (hsize_t) index,
                                                  NULL,
                                                  0,
                                                  H5P_DEFAULT);
    if (name_len > 0) {
        char* name = new char[name_len+1];
        name_len = H5Lget_name_by_idx(hid,
                                      ".",
                                      H5_INDEX_NAME,
                                      H5_ITER_NATIVE,
                                      (hsize_t) index,
                                      name,
                                      name_len+1,
                                      H5P_DEFAULT);
        str_name = name;
        delete [] name;
    } else {
        throw H5Exception("objectName: No object found, H5Lget_name_by_idx returned no name");
    }

    return str_name;
}


bool Group::hasData(const std::string &name) const {
    return hasObject(name) && objectOfType(name, H5O_TYPE_DATASET);
}


void Group::removeData(const std::string &name) {
    if (hasData(name))
        H5Gunlink(hid, name.c_str());
}

DataSet Group::createData(const std::string &name,
            const H5::DataType &fileType,
            const H5::DataSpace &fileSpace, const H5::DSetCreatPropList &cpList) const {
    hid_t id = H5Dcreate(hid, name.c_str(), fileType.getId(), fileSpace.getId(), H5P_DEFAULT, cpList.getId(), H5P_DEFAULT);
    return DataSet(H5::DataSet(id));
}

DataSet Group::openData(const std::string &name) const {
    hid_t did = H5Dopen(hid, name.c_str(), H5P_DEFAULT);
    H5::DataSet ds5(did);
    return DataSet(ds5);
}


bool Group::hasGroup(const std::string &name) const {
    return hasObject(name) && objectOfType(name, H5O_TYPE_GROUP);
}


Group Group::openGroup(const std::string &name, bool create) const {
    if(!util::nameCheck(name)) throw InvalidName("openGroup");
    Group g;
    if (hasGroup(name)) {
        hid_t gid = H5Gopen(hid, name.c_str(), H5P_DEFAULT);
        g = Group(gid);
        H5Idec_ref(gid);
    } else if (create) {
        hid_t gcpl = H5Pcreate(H5P_GROUP_CREATE);

        if (gcpl < 0) {
            throw H5Exception("Unable to create group with name '" + name + "'! (H5Pcreate)");
        }

        //we want hdf5 to keep track of the order in which links were created so that
        //the order for indexed based accessors is stable cf. issue #387
        herr_t res = H5Pset_link_creation_order(gcpl, H5P_CRT_ORDER_TRACKED|H5P_CRT_ORDER_INDEXED);
        H5Error::check(res, "Unable to create group with name '" + name + "'! (H5Pset_link_cr...)");

        hid_t h5_gid = H5Gcreate2(hid, name.c_str(), H5P_DEFAULT, gcpl, H5P_DEFAULT);
        H5Pclose(gcpl);
        if (h5_gid < 0) {
            throw H5Exception("Unable to create group with name '" + name + "'! (H5Gcreate2)");
        }
        g = Group(H5::Group(h5_gid));
    } else {
        throw H5Exception("Unable to open group with name '" + name + "'!");
    }
    return g;
}


optGroup Group::openOptGroup(const std::string &name) {
    if(!util::nameCheck(name)) throw InvalidName("openOptGroup");
    return optGroup(*this, name);
}


void Group::removeGroup(const std::string &name) {
    if (hasGroup(name))
        H5Gunlink(hid, name.c_str());
}


void Group::renameGroup(const std::string &old_name, const std::string &new_name) {
    if(!util::nameCheck(new_name)) throw InvalidName("renameGroup");
    if (hasGroup(old_name)) {
        H5Gmove(hid, old_name.c_str(), new_name.c_str()); //FIXME: H5Gmove is deprecated
    }
}


H5::Group Group::h5Group() const {
    if (H5Iis_valid(hid)) {
        H5Iinc_ref(hid);
        return H5::Group(hid);
    } else {
        return H5::Group();
    }
}


Group Group::createLink(const Group &target, const std::string &link_name) {
    if(!util::nameCheck(link_name)) throw InvalidName("createLink");
    herr_t error = H5Lcreate_hard(target.hid, ".", hid, link_name.c_str(),
                                  H5L_SAME_LOC, H5L_SAME_LOC);
    H5Error::check(error, "Unable to create link " + link_name);
    return openGroup(link_name, false);
}

// TODO implement some kind of roll-back in order to avoid half renamed links.
bool Group::renameAllLinks(const std::string &old_name, const std::string &new_name) {
    if(!util::nameCheck(new_name)) throw InvalidName("renameAllLinks");
    bool renamed = false;

    if (hasGroup(old_name)) {
        std::vector<std::string> links;

        Group  group     = openGroup(old_name, false);
        size_t size      = 128;
        char *name_read  = new char[size];

        size_t size_read = H5Iget_name(group.hid, name_read, size);
        while (size_read > 0) {

            if (size_read < size) {
                H5Ldelete(hid, name_read, H5L_SAME_LOC);
                links.push_back(name_read);
            } else {
                delete[] name_read;
                size = size * 2;
                name_read = new char[size];
            }

            size_read = H5Iget_name(group.hid, name_read, size);
        }

        renamed = links.size() > 0;
        for (std::string curr_name: links) {
            size_t pos = curr_name.find_last_of('/') + 1;

            if (curr_name.substr(pos) == old_name) {
                curr_name.replace(curr_name.begin() + pos, curr_name.end(), new_name.begin(), new_name.end());
            }

            herr_t error = H5Lcreate_hard(group.hid, ".", hid, curr_name.c_str(),
                                          H5L_SAME_LOC, H5L_SAME_LOC);

            renamed = renamed && (error >= 0);
        }
    }

    return renamed;
}

// TODO implement some kind of roll-back in order to avoid half removed links.
bool Group::removeAllLinks(const std::string &name) {
    bool removed = false;

    if (hasGroup(name)) {
        Group  group      = openGroup(name, false);
        size_t size       = 128;
        char *name_read   = new char[size];

        size_t size_read  = H5Iget_name(group.hid, name_read, size);
        while (size_read > 0) {
            if (size_read < size) {
                H5Ldelete(hid, name_read, H5L_SAME_LOC);
            } else {
                delete[] name_read;
                size = size * 2;
                name_read = new char[size];
            }
            size_read = H5Iget_name(group.hid, name_read, size);
        }

        delete[] name_read;
        removed = true;
    }

    return removed;
}


boost::optional<Group> Group::findGroupByNameOrAttribute(std::string const &attr, std::string const &value) const {

    if (hasObject(value)) {
        return boost::make_optional(openGroup(value, false));
    } else if (util::looksLikeUUID(value)) {
        return findGroupByAttribute(attr, value);
    } else {
        return boost::optional<Group>();
    }
}


boost::optional<DataSet> Group::findDataByNameOrAttribute(std::string const &attr, std::string const &value) const {

    if (hasObject(value)) {
        return boost::make_optional(openData(value));
    } else if (util::looksLikeUUID(value)) {
        return findDataByAttribute(attr, value);
    } else {
        return boost::optional<DataSet>();
    }
}


Group::~Group()
{}

} // namespace hdf5
} // namespace nix
