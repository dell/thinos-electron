function resolveSingleObjectGetters(object) {
  if (object && typeof object === 'object') {
    const newObject = {};
    // eslint-disable-next-line guard-for-in
    for (const key in object) {
      newObject[key] = resolveGetters(object[key])[0];
    }
    return newObject;
  }
  return object;
}

function resolveGetters(...args) {
  return args.map(resolveSingleObjectGetters);
}

module.exports = {
  resolveGetters,
};
