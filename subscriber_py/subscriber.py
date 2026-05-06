"""
Virtual Heat Pump Subscriber
Receives power limit command from DSO via OPC UA PubSub
"""

import asyncio
import logging
from dataclasses import dataclass, field

from asyncua import Node, Server, pubsub, ua


@dataclass
class PubSubCFG:
    PublisherID: ua.Variant = field(default_factory=lambda: ua.Variant(ua.UInt16(2234)))
    WriterId: ua.UInt16 = field(default_factory=lambda: ua.UInt16(100))
    DataSetWriterId: ua.UInt16 = field(default_factory=lambda: ua.UInt16(62541))
    Url: str = "opc.udp://224.0.0.22:4840"


CFG = PubSubCFG()


def create_meta_data():
    dataset = pubsub.DataSetMeta.Create("Demo PDS")
    dataset.add_scalar("PowerLimit", ua.VariantType.String)
    return dataset


async def init_pubsub_connection(server: Server, nodes: list[Node]) -> pubsub.PubSubConnection:
    metadata = create_meta_data()
    subscribed_ds = pubsub.SubScribedTargetVariables(
        server,
        [
            pubsub.FieldTargets.createTarget(
                metadata.get_field((await n.read_browse_name()).Name), n.nodeid
            )
            for n in nodes
        ],
    )
    reader = pubsub.ReaderGroup.new(
        name="ReaderGroup1",
        enable=True,
        reader=[
            pubsub.DataSetReader.new(
                CFG.PublisherID,
                CFG.WriterId,
                CFG.DataSetWriterId,
                metadata,
                name="SimpleDataSetReader",
                subscribed=subscribed_ds,
                enabled=True,
            )
        ],
    )
    return pubsub.PubSubConnection.udp_uadp(
        "Subscriber Connection1 UDP UADP",
        ua.UInt16(2),
        pubsub.UdpSettings(Url=CFG.Url),
        reader_groups=[reader],
    )


async def create_variables(node: Node, ns: ua.UInt16) -> list[Node]:
    folder = await node.add_folder(ua.NodeId("SubscriberDemo", ns), "PublisherDemoNodes")
    return [
        await folder.add_variable(
            ua.NodeId("SubPowerLimit", ns), "PowerLimit", "", ua.VariantType.String
        ),
    ]


async def monitor_variables(nodes: list[Node]):
    last_val = None
    while True:
        try:
            val = await nodes[0].read_value()
            if val != last_val and val != "":
                print("\n" + "=" * 55)
                print("[HEAT PUMP] Command received from DSO!")
                print(f"  Raw command  : {val}")
                if "PLimit" in val:
                    limit = val.split(":")[1].replace("}", "")
                    print(f"  Power limit  : {float(limit)/1000:.2f} kW")
                print("=" * 55)
                last_val = val
        except Exception:
            pass
        await asyncio.sleep(1)


async def main():
    logging.basicConfig(level=logging.WARNING)
    server = Server()
    await server.init()
    server.set_endpoint("opc.tcp://0.0.0.0:4841/freeopcua/server/")

    uri = "http://examples.freeopcua.github.io"
    idx = await server.register_namespace(uri)
    nodes = await create_variables(server.nodes.objects, idx)
    ps = await server.get_pubsub()

    print("[HEAT PUMP] Virtual Heat Pump Subscriber started")
    print("[HEAT PUMP] Waiting for DSO power limit command...")

    async with server:
        connection = await init_pubsub_connection(server, nodes)
        await ps.add_connection(connection)
        await ps.init_information_model()
        await ps.start()
        await monitor_variables(nodes)


if __name__ == "__main__":
    asyncio.run(main())