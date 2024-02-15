| :warning: **Speedometer 3 is in active development and is unstable**. You can follow along with development in this repository, but see [Speedometer 2.1](https://browserbench.org/Speedometer2.1/) for the latest stable version. |
| ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

# What is Speedometer?

Speedometer is a benchmark for web browsers that measures Web application responsiveness
by timing simulated user interactions on various workloads. Our primary goal is to make it
reflect the real-world Web as much as possible. When a browser improves its score on the
benchmark, actual users should benefit. In order to achieve this, it should:

-   Test end-to-end user journeys instead of testing specific features in a tight loop. Each
    test should exercise the full set of what’s needed from the engine in order for a user to
    accomplish a task.
-   Evolve over time, adapting to the present Web on a regular basis. This should be informed
    by current usage data, and by consensus about features which are important for engines to
    optimize to provide a consistent experience for users and site authors.
-   Be accessible to the public and useful to browser engineers. It should run in every modern
    browser by visiting a normal web page. It should run relatively quickly, while providing
    enough test coverage to be reflective of the real-world Web.
