namespace meta {

class object {
public:
  constexpr object(__metaobject_id mo) noexcept : _mo{mo} {}

  constexpr bool is_named() const noexcept {
    return __metaobject_is_meta_named(_mo);
  }

  constexpr bool is_type() const noexcept {
    return __metaobject_is_meta_type(_mo);
  }

  constexpr bool is_scope() const noexcept {
    return __metaobject_is_meta_scope(_mo);
  }

protected:
  const __metaobject_id _mo;
};

class named : public object {
public:
  using object::object;

  constexpr auto base_name_length() const noexcept {
    return __metaobject_base_name_len(_mo);
  }

  /*
  constexpr const char *get_base_name() const noexcept {
    return __metaobject_get_base_name(_mo);
  }
  */
};

} // namespace meta

int main(void) {

  const meta::named moint(__reflexpr(int));

  static_assert(moint.is_named());
  static_assert(moint.is_type());
  static_assert(!moint.is_scope());
  static_assert(moint.base_name_length() > 0);

  return 0;
}
