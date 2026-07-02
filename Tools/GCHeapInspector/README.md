# GC Heap Inspector

This inspector is a browser-based application for inspecting GC heap snapshots generated
by `notifyutil -p com.apple.WebKit.dumpGCHeap` and saved as a `.json` file. For a detailed
description of how to save a snapshot, see the opening screen of the inspector.

## Opening the inspector

In this directory, execute

    python -m SimpleHTTPServer

then in a web browser, go to `http://localhost:8000/gc-heap-inspector.html`,
then drag and drop a snapshot file onto the indicated box.

## Using the inspector

Once a snapshot is loaded, the inspector displays an overview of the objects
in the snapshot. The overview is divided into the following sections:

### Details

This section is initially empty. It can be populated with tools showing detailed
information about certain objects, such as:
    - All paths from roots to a particular node.
    - A node explorer for navigating the edges of the object graph around a
      particular node.
    - Dominator summary of instances of a particular type.

### Important objects

This section lists all instances of `Window` and `HTMLDocument`. These two types are
chosen because of their importance in the typical memory graph. Accidentally retaining
them can retain a large amount of memory.

In this section as well as everywhere else in the inspector, an object is displayed as
its class name and the node ID in the graph, followed by other details. For example:

    Window#543 (retains 0.5K) [Show all paths] ... etc

The node ID is a unique identifier of the object in the memory snapshot. The same object
appearing elsewhere in the inspector will show the same node ID. A different instance of
the same class will have a different node ID. Clicking the node ID adds the **Node
Explorer** tool (explained in a dedicated section below) for that object to the **Tools**
section.

Clicking an object in this section expands it into the shortest path to it from a GC
root. The path is presented as a list of objects with the GC root at the top and the
window or the document at the bottom.

The **Show all paths** button adds a tool to the **Tools** section that displays all (as
opposed to the shortest) paths to the object from GC roots.

### Roots

Lists all types whose instances are registered as GC roots, with the number of root
instances of each type. Each type entry expands into a list of instances.

### All objects by type

This section lists all types, with the number of instances and their combined memory
footprint. Expanding a type lists the individual instances.

The memory footprint of a type is followed by the `Dominators` button. The button adds
an immediate dominator summary for this type to the **Tools** section. See below for
the explanation of the dominator summary.

## Details

### All paths to object

This tool is added by clicking the **Show all paths** button in an object. It lists all
possible paths to the object from GC roots. Each element of the list represents one path.
Expanding a path shows its individual steps.

### Node Explorer

A node explorer can be opened on any object by clicking the node ID. The explorer
features three expandable entries:

1. The shortest path to the node, expandable to a list of steps from the closest GC root, just
like the shortest paths displayed in the **Important objects** section.
2. Owners of the node are the incoming edges of the node graph. Each owner can be expanded
to reveal its owners, and so on.
3. "Owned by" is a list of outgoing edges of the node graph. Just like with owners, an
owned object can be expanded to reveal the objects it owns.

### Dominator summary

The dominator summary tool goes through a list of instances of a type and collects
information about their immediate dominators. For each dominator, it counts how many
instances it dominates. The dominators are then listed in the reverse order of the counts.

Dominator information can be useful in finding GC object leaks that happen because objects
are added to a registry of some kind and never removed. If a large group of unexplainably
live objects have the same dominator, the dominator is a prime suspect for being the
reason those objects are staying alive.

The inspector presents dominator information in two forms. One is the dominator summary.
The other is a list of "interesting" dominators of an object. Interesting dominators
are dominators other than the object itself and a GC root. If an object has interesting
dominators, they are always displayed when displaying the object.
