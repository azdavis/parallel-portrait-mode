# 15-418 Final Project Proposal: Portrait Mode

## Summary

We are going to build portrait mode with bokeh blur on NVIDIA GPUs.

Portrait mode has been popularized largely by high end DSLR cameras, where the
foreground of the subject is in focus and the background is blurred by bokeh
shapes. Recently, smartphone cameras without specialized hardware have gained
the ability to take portrait mode pictures by using software-only background
detection and manipulation methods, which is what we will implement.

## Background

There are two main stages to our application, extracting the background and
blurring.

We will extract the background using active contour model, known as the snake
method. Essentially, we will calculate an energy for each pixel in the image.
The energy is a weighted sum of the filtered intensity of the pixel and edge
value of a pixel, calculated with the Canny Edge detector or Marr Hildreth. We
will use the energy values to iteratively update snakes, a set of points that
move from the outer edge to the center based on the difference in energies.

Additionally, each snake has internal energy that prevents points from being
too far apart and prevents oscillations in the points. At the termination of
the algorithm, the snakes will have fit the foreground of the image, which is
where the energy difference is the highest.

The second step is to apply the bokeh blur. We will apply a filter in the shape
of a polygon across an image. Values are accumulated by shifting the image in a
shape and computing a weighted sum.

Repeat the following steps before snake converges, when each point finds a
energy difference above a threshold.

1.  Calculate energy values for each pixel
    1.  Get grayscale image by averaging RGB values of each pixel.
        (Highly parallelizable across pixels)
    2.  Calculate edges by convolving a gaussian filter across the image.
        (Highly parallelizable across pixels)
2.  Move each point of snake based on energy value. (Parallelizable across
    point of snake)
3.  Compute internal energy of snake. (Requires synchronization within a snake)

Then, when converged: Convolve the image with the bokeh filter. (Highly
parallelizable across pixels)

## The Challenge

A challenge is to use parallelism to speed up each step of the portrait mode
process. A bottleneck in one process can significantly reduce speedup across
the entire process due to Amdahl's law. Also, we have to launch and synchronize
CUDA cores for each step, which takes time.

Another restriction is that each of the points of the snake have to be
synchronized in order to maintain the internal energy of the snake. This allows
for different snake mappings of spatial locality in the image versus each point
of the snake.

Overall, many of the steps are computationally heavy, with the convolutions of
filters and calculating gradients in the image. Because we are processing the
entire image, spatially dividing it up will allow for locality.

## Resources

We will use the Gates cluster machines with the GPUs. We will follow the snake
implementation outlined in the paper by Kass, Witkin, and Terzopoulous. We are
also planning on using the starter code in the github repo by adl1995 for
implementations of Canny-Edge and Marr Hildreth. To implement the Bokeh effect,
we will follow the starter code in the tutorial.

## Goals and Deliverables

We plan to be able to implement portrait mode with kernels on a GPU. Any image
will be taken as an input and the output would make it look like the image was
originally taken in portrait mode. By using GPUs to for many of the pixel
operations, we hope to be significantly faster than sequential methods.

If we have time, we hope to adapt this algorithm to work for videos or gifs.
Ideally, we can achieve realtime portrait mode in 30 fps. (30ms per image)

## Platform Choice

We will run this on GPUs, which are ideal for image processing. We will do our
development, testing, and benchmarking on the GHC machines.

## Schedule

| Date       | Item                                                     |
|------------|----------------------------------------------------------|
| 2018-04-11 | Proposal                                                 |
| 2018-04-15 | Sequential Python: background detection and manipulation |
| 2018-04-20 | Sequential C: background detection and manipulation      |
| 2018-05-05 | Parallel CUDA: background detection                      |
| 2018-05-06 | Parallel CUDA: background manipulation                   |
| 2018-05-07 | Writeup and poster                                       |
| 2018-05-08 | Presentation                                             |

## Citations

- https://github.com/adl1995/edge-detectors
- http://www.cs.ait.ac.th/~mdailey/cvreadings/Kass-Snakes.pdf
- https://www.scratchapixel.com/lessons/digital-imaging/simple-image-manipulations/bookeh-effect
