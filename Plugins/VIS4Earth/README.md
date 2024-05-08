# VIS for Earth

VIS4Earth is a library enabling earth-scaled Visualization (mainly Scientific Visualization) in UE 5 with Cesium.

## Algorithm

### Relation between $(B,L,H)$ in Cesium-ECEF and $(x,y,z)$ in UE

$$
\begin{align*}
z &= kH \sin{L}\\
x &= H \cos{L} \cos{B}\\
y &= H \cos{L} \sin{B}\\
\\
H &= \sqrt{x^2 + y^2 + \frac{1}{k^2}z^2}\\
h(L) &= \sqrt{x^2 + y^2 + z^2} = H\sqrt{\cos^2{L} + k^2\sin^2{L}}\\
&= H\sqrt{1+(k^2-1)\sin^2{L}}\\
L &= \arcsin{\frac{z}{kH}}\\
B &= \arcsin{\frac{y}{H\cos{L}}} = \arctan{\frac{y}{x}}
\end{align*}
$$

where
- $H$ is the height-to-center (radius) above the Equator.
- $k$ is $\frac{Radius_{short}}{Radius_{long}}$.
- $h(L)$ is the height-to-center at the latitude $L$.

#### Implementation

[GeoMath.ush](./Shaders/GeoMath.ush)

### Intersect Ray $\vec{o}+t\vec{d}$ with Sphere S(H,k)

$$
\begin{align*}
\begin{bmatrix}
x\\
y\\
z
\end{bmatrix}
&=
\begin{bmatrix}
\vec{o}_x + t\vec{d}_x\\
\vec{o}_y + t\vec{d}_y\\
\vec{o}_z + t\vec{d}_z
\end{bmatrix}\\
\frac{x^2}{H^2} + \frac{y^2}{H^2} + \frac{z^2}{(kH)^2} &= 1\\
&\to\\
(\vec{d}_x^2 + \vec{d}_y^2 + \frac{1}{k^2}\vec{d}_z^2) * t^2 +\\
2(\vec{o}_x\vec{d}_x + \vec{o}_y\vec{d}_y + \frac{1}{k^2}\vec{o}_z\vec{d}_z) * t +\\
(\vec{o}_x^2 + \vec{o}_y^2 + \frac{1}{k^2}\vec{o}_z^2) - H^2 &= 0\\
&\to\\
\Delta &= b^2-4ac\\
t &= \frac{-b \pm \sqrt{\Delta}}{2a}
\end{align*}
$$

#### Implementation

[GeoMath.ush](./Shaders/GeoMath.ush)
