/********************************************************************************
 *    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ASIOFI_PASSIVE_ENDPOINT_HPP
#define ASIOFI_PASSIVE_ENDPOINT_HPP

#include <asiofi/errno.hpp>
#include <asiofi/event_queue.hpp>
#include <asiofi/fabric.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/io_context_strand.hpp>
#include <functional>
#include <iostream>
#include <rdma/fi_cm.h>
#include <rdma/fi_endpoint.h>
#include <utility>

namespace asiofi
{
  /**
   * @struct passive_endpoint connected_endpoint.hpp <asiofi/connected_endpoint.hpp>
   * @brief Wraps fid_pep
   */
  struct passive_endpoint
  {
    /// get wrapped C object
    friend auto get_wrapped_obj(const passive_endpoint& pep) -> fid_pep* { return pep.m_pep.get(); }

    /// ctor #1
    explicit passive_endpoint(boost::asio::io_context& io_context, const fabric& fabric)
    : m_fabric(fabric)
    , m_io_context(io_context)
    , m_eq(io_context, fabric)
    , m_pep(create_passive_endpoint(fabric, m_context))
    {
      // bind event queue to passive connected_endpoint registering for connection requests
      bind(m_eq, passive_endpoint::eq_flag::connreq);
    }

    /// (default) ctor
    explicit passive_endpoint() = delete;

    /// copy ctor
    explicit passive_endpoint(const passive_endpoint& rh) = delete;

    /// move ctor
    explicit passive_endpoint(passive_endpoint&& rhs) = default;

    /// dtor
    ~passive_endpoint()
    {
    }

    enum class eq_flag : uint64_t {
      connreq = FI_CONNREQ
    };

    auto bind(const event_queue& eq, passive_endpoint::eq_flag flags) -> void
    {
      auto rc = fi_pep_bind(m_pep.get(), &get_wrapped_obj(eq)->fid, static_cast<uint64_t>(flags));
      if (rc != FI_SUCCESS)
        throw runtime_error("Failed binding ofi event queue to ofi passive connected_endpoint, reason: ", fi_strerror(rc));
    }

    /// Listen for connection requests
    template<typename CompletionHandler>
    auto listen(CompletionHandler&& handler) -> void
    {
      auto rc = fi_listen(m_pep.get());
      if (rc != 0)
        throw runtime_error("Failed listening on ofi passive_ep, reason: ", fi_strerror(rc));

      m_eq.read([&, _handler = std::forward<CompletionHandler>(handler)](eq::event event, fid_t handle, info&& info){
        if (event == eq::event::connreq) {
          _handler(handle, std::move(info));
        } else {
          throw runtime_error("Unexpected event read from ofi event queue, ",
                              "expected: ", static_cast<uint32_t>(eq::event::connreq), " (event::connreq), ",
                              "got: ", static_cast<uint32_t>(event));
        }
      });
    }

    auto reject(fid_t handle) -> void
    {
      auto rc = fi_reject(m_pep.get(), handle, nullptr, 0);
      if (rc != 0)
        throw runtime_error("Failed rejecting connection request (", handle, ") on ofi passive connected_endpoint, reason: ", fi_strerror(rc));
    }

    private:
    using fid_pep_deleter = std::function<void(fid_pep*)>;

    const fabric& m_fabric;
    fi_context m_context;
    boost::asio::io_context& m_io_context;
    event_queue m_eq;
    std::unique_ptr<fid_pep, fid_pep_deleter> m_pep;

    static auto create_passive_endpoint(const fabric& fabric, fi_context& context) -> std::unique_ptr<fid_pep, fid_pep_deleter> 
    {
      fid_pep* pep;
      auto rc = fi_passive_ep(get_wrapped_obj(fabric),
                              get_wrapped_obj(fabric.get_info()),
                              &pep,
                              &context);
      if (rc != 0)
        throw runtime_error("Failed creating ofi passive connected_endpoint, reason: ", fi_strerror(rc));

      return {pep, [](fid_pep* pep){ fi_close(&pep->fid); }};
    }
  }; /* struct passive_endpoint */

  using pep = passive_endpoint;

} /* namespace asiofi */

#endif /* ASIOFI_PASSIVE_ENDPOINT_HPP */
