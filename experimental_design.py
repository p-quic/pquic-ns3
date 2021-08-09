"""
    Functions for generating values according to a WSP file and the Experimental Design method
"""
from itertools import chain


def flatten(l):
    """
        inefficiently flattens a list
        l: an arbitrary list
    """
    if not l:
        return list(l)
    r = []
    for e in l:
        if not isinstance(e, list):
            r.append(e)
        else:
            r.extend(flatten(e))
    return r


def load_wsp(filename, nrows, ncols):
    # Open the file
    f = open("%s" % filename)
    lines = f.readlines()
    f.close()

    # The interesting line is the third one
    line = lines[2]
    split_line = line.split(",")
    nums = []

    for x in split_line:
        nums.append(float(x))
    # print(len(split_line))
    # print(len(nums))

    if len(nums) != nrows*ncols:
        raise Exception("wrong number of elements in wsp matrix: %d instead of %d(with %d rows)" % (len(nums), nrows*ncols, nrows))

    # print("load matrix")

    # The matrix is encoded as an array of nrowsxncols
    matrix = []
    for i in range(nrows):
        row = []
        for j in range(ncols):
            try:
                row.append(nums[i * ncols + j])
            except:
                print((i * ncols + j))
                raise

        matrix.append(row)

    return matrix

class ParamsGenerator(object):
    def __init__(self, params_values, matrix):
        self.index = 0
        self.params_values = params_values
        for k in ('delay_ms_a', 'delay_ms_b'):
            if isinstance(params_values.get(k, None), list):
                for i in range(len(params_values[k])):
                    params_values["%s_%d" % (k, i)] = params_values[k][i]
                params_values.pop(k, None)
        self.param_names = list(sorted(params_values.keys()))
        self.ranges_full_name = {self._full_name(key, val.get("count", 1)): val["range"] for key, val in list(params_values.items())}
        names = []
        for n in list(params_values.keys()):
            for key in list(params_values[n]["range"].keys()) if isinstance(params_values[n]["range"], dict) else [None]:
                names.append((n, key))
        self.param_full_names = sorted(flatten([[self._full_name(name_key[0], i, name_key[1]) for i in range(params_values[name_key[0]].get("count", 1))] for name_key in names]))
        # decide for an arbitrary ordering of the parameters
        self.params_indexes = {self.param_full_names[i]: i for i in range(len(self.param_full_names))}
        self.matrix = matrix

    def _full_name(self, name, count, key=None):
        if self.params_values[name].get("count", 1) > 1:
            return "%s_%d%s" % (name, count, ("_%s" % str(key)) if key is not None else "")
        return "%s%s" % (name, ("_%s" % str(key)) if key is not None else "")

    def generate_value(self):
        retval = self._generate_value_at(self.index)
        self.index += 1
        return retval

    def _generate_value_at(self, i):
        retval = {}
        for name in self.param_names:
            retval[name] = []
            for count in range(self.params_values[name].get("count", 1)):
                param_range = self.params_values[name]["range"]
                if isinstance(param_range, dict):
                    to_append = {key: self.params_values[name]["type"](
                        self.matrix[self.params_indexes[self._full_name(name, count, key)]][i] * (param_range[key][1] - param_range[key][0]) + param_range[key][0])
                        for key in list(param_range.keys())}
                else:
                    full_name = self._full_name(name, count)
                    param_index = self.params_indexes[full_name]
                    float_value = self.matrix[param_index][i]
                    to_append = self.params_values[name]["type"](float_value * (param_range[1] - param_range[0]) + param_range[0])
                retval[name].append(to_append)
        return retval

    def __len__(self):
        return len(self.matrix[0])

    def generate_all_values(self):
        for i in range(len(self.matrix[0])):
            yield self._generate_value_at(i)